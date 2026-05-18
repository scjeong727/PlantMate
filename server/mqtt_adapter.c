#include <arpa/inet.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "event_repository.h"
#include "db_queue.h"
#include "event_log.h"
#include "mqtt_device_registry.h"
#include "mqtt_adapter.h"
#include "mqtt_topic_helper.h"
#include "plant_owner_cache.h"
#include "plant_service.h"
#include "plant_threshold_cache.h"
#include "sensor_buffer.h"
#include "sensor_event_dispatcher.h"
#include "sensor_repository.h"
#include "sensor_service.h"
#include "user_service.h"

#define MQTT_PORT 1883
#define MQTT_MAX_CLIENTS FD_SETSIZE
#define MQTT_MAX_PACKET 8192
#define MQTT_MAX_SUBS 16

extern SensorBuffer g_sensor_buffer;
extern DBQueue g_db_queue;
extern EventLog g_event_log;

typedef struct {
    int sock;
    int in_use;
    char client_id[128];
    char subscriptions[MQTT_MAX_SUBS][128];
    int subscription_count;
} MqttClient;

static MqttClient g_clients[MQTT_MAX_CLIENTS];
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static int recv_full(int sock, unsigned char* buf, int len)
{
    int received = 0;

    while (received < len) {
        int n = recv(sock, buf + received, len - received, 0);
        if (n <= 0)
            return 0;
        received += n;
    }

    return 1;
}

static int mqtt_read_remaining_length(int sock, int* out_len)
{
    int multiplier = 1;
    int value = 0;
    unsigned char encoded = 0;
    int count = 0;

    do {
        if (!recv_full(sock, &encoded, 1))
            return 0;
        value += (encoded & 127) * multiplier;
        multiplier *= 128;
        count++;
    } while ((encoded & 128) != 0 && count < 4);

    *out_len = value;
    return 1;
}

static int mqtt_send_all(int sock, const unsigned char* buf, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0)
            return 0;
        sent += (size_t)n;
    }

    return 1;
}

static int mqtt_encode_remaining_length(int value, unsigned char* out)
{
    int count = 0;

    do {
        unsigned char byte = (unsigned char)(value % 128);
        value /= 128;
        if (value > 0)
            byte |= 0x80;
        out[count++] = byte;
    } while (value > 0 && count < 4);

    return count;
}

static void mqtt_close_client(MqttClient* client)
{
    if (!client || !client->in_use)
        return;

    close(client->sock);
    memset(client, 0, sizeof(*client));
}

static MqttClient* mqtt_alloc_client(int sock)
{
    int i;

    for (i = 0; i < MQTT_MAX_CLIENTS; ++i) {
        if (!g_clients[i].in_use) {
            memset(&g_clients[i], 0, sizeof(g_clients[i]));
            g_clients[i].sock = sock;
            g_clients[i].in_use = 1;
            return &g_clients[i];
        }
    }

    return NULL;
}

static int mqtt_write_publish(int sock, const char* topic, const char* payload)
{
    unsigned char packet[MQTT_MAX_PACKET];
    unsigned char remaining[4];
    size_t topic_len = strlen(topic);
    size_t payload_len = strlen(payload);
    size_t pos = 0;
    int remaining_len_size;
    int remaining_len;

    remaining_len = (int)(2 + topic_len + payload_len);
    packet[pos++] = 0x30;
    remaining_len_size = mqtt_encode_remaining_length(remaining_len, remaining);
    memcpy(packet + pos, remaining, (size_t)remaining_len_size);
    pos += (size_t)remaining_len_size;

    packet[pos++] = (unsigned char)((topic_len >> 8) & 0xFF);
    packet[pos++] = (unsigned char)(topic_len & 0xFF);
    memcpy(packet + pos, topic, topic_len);
    pos += topic_len;
    memcpy(packet + pos, payload, payload_len);
    pos += payload_len;

    return mqtt_send_all(sock, packet, pos);
}

static void mqtt_publish_to_subscribers(const char* topic, const char* payload)
{
    int i;
    int j;

    pthread_mutex_lock(&g_clients_mutex);
    for (i = 0; i < MQTT_MAX_CLIENTS; ++i) {
        if (!g_clients[i].in_use)
            continue;

        for (j = 0; j < g_clients[i].subscription_count; ++j) {
            if (strcmp(g_clients[i].subscriptions[j], topic) == 0) {
                if (!mqtt_write_publish(g_clients[i].sock, topic, payload)) {
                    mqtt_close_client(&g_clients[i]);
                }
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

static void mqtt_publish_app_response(const MqttClient* client, const char* payload)
{
    char topic[192];

    if (!client || !payload)
        return;

    mqtt_topic_build_app_response(client->client_id, topic, sizeof(topic));
    if (topic[0] == '\0')
        return;

    mqtt_publish_to_subscribers(topic, payload);
}

static const char* mqtt_skip_ok_prefix(const char* response)
{
    const char* payload = response;

    if (!payload)
        return "{}";

    if (strncmp(payload, "OK", 2) == 0) {
        payload += 2;
        while (*payload == ' ' || *payload == '\t' || *payload == '\n')
            payload++;
    }

    return *payload ? payload : "{}";
}

static void mqtt_publish_rpc_ok_raw(const MqttClient* client, const char* request_id, const char* raw_json)
{
    char payload[MQTT_MAX_PACKET];
    const char* safe_request_id = request_id ? request_id : "";
    const char* safe_json = raw_json && *raw_json ? raw_json : "{}";

    snprintf(payload, sizeof(payload),
        "{\"requestId\":\"%s\",\"ok\":true,\"data\":%s}",
        safe_request_id, safe_json);
    mqtt_publish_app_response(client, payload);
}

static void mqtt_publish_rpc_error(const MqttClient* client, const char* request_id, const char* error)
{
    char payload[MQTT_MAX_PACKET];
    const char* safe_request_id = request_id ? request_id : "";
    const char* safe_error = error && *error ? error : "request_failed";

    snprintf(payload, sizeof(payload),
        "{\"requestId\":\"%s\",\"ok\":false,\"error\":\"%s\"}",
        safe_request_id, safe_error);
    mqtt_publish_app_response(client, payload);
}

void mqtt_adapter_publish_sensor(int plant_id, const char* payload)
{
    char topic[64];

    if (plant_id <= 0 || !payload)
        return;

    mqtt_topic_build_plant_sensor(plant_id, topic, sizeof(topic));
    mqtt_publish_to_subscribers(topic, payload);
}

void mqtt_adapter_publish_status(int plant_id, const char* payload)
{
    char topic[64];

    if (plant_id <= 0 || !payload)
        return;

    mqtt_topic_build_plant_status(plant_id, topic, sizeof(topic));
    mqtt_publish_to_subscribers(topic, payload);
}

void mqtt_adapter_publish_device_command(const char* device_type, const char* device_id, const char* action, const char* payload)
{
    char topic[128];

    if (!payload)
        return;

    mqtt_topic_build_device_command(device_type, device_id, action, topic, sizeof(topic));
    if (topic[0] == '\0')
        return;

    mqtt_publish_to_subscribers(topic, payload);
}

void mqtt_adapter_publish_device_status(const char* device_type, const char* device_id, const char* payload)
{
    char topic[128];

    if (!payload)
        return;

    mqtt_topic_build_device_status(device_type, device_id, topic, sizeof(topic));
    if (topic[0] == '\0')
        return;

    mqtt_publish_to_subscribers(topic, payload);
}

static int json_extract_string(const char* json, const char* key, char* out, size_t out_size)
{
    char pattern[64];
    const char* pos;
    const char* start;
    const char* end;
    size_t len;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = strstr(json, pattern);
    if (!pos)
        return 0;

    start = strchr(pos + strlen(pattern), ':');
    if (!start)
        return 0;
    start++;
    while (*start == ' ' || *start == '\t')
        start++;
    if (*start != '"')
        return 0;
    start++;

    end = strchr(start, '"');
    if (!end)
        return 0;

    len = (size_t)(end - start);
    if (len >= out_size)
        len = out_size - 1;

    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int json_extract_int(const char* json, const char* key, int* out)
{
    char pattern[64];
    const char* pos;
    const char* start;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = strstr(json, pattern);
    if (!pos)
        return 0;

    start = strchr(pos + strlen(pattern), ':');
    if (!start)
        return 0;
    start++;
    while (*start == ' ' || *start == '\t')
        start++;

    return sscanf(start, "%d", out) == 1;
}

static int json_extract_double(const char* json, const char* key, double* out)
{
    char pattern[64];
    const char* pos;
    const char* start;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = strstr(json, pattern);
    if (!pos)
        return 0;

    start = strchr(pos + strlen(pattern), ':');
    if (!start)
        return 0;
    start++;
    while (*start == ' ' || *start == '\t')
        start++;

    return sscanf(start, "%lf", out) == 1;
}

static int parse_plant_topic(const char* topic, int* plant_id, char* suffix, size_t suffix_size)
{
    char rest[64];

    if (sscanf(topic, "plant/%d/%63s", plant_id, rest) != 2)
        return 0;

    snprintf(suffix, suffix_size, "%s", rest);

    if (strcmp(rest, "sensor") == 0 || strcmp(rest, "status") == 0)
        return 1;

    if (sscanf(topic, "plant/%d/%63[^/]/%63s", plant_id, rest, suffix) == 3)
        return 1;

    return 0;
}

static int mqtt_is_app_request_topic(const MqttClient* client, const char* topic)
{
    char expected[192];

    if (!client || !topic)
        return 0;

    mqtt_topic_build_app_request(client->client_id, expected, sizeof(expected));
    return expected[0] != '\0' && strcmp(topic, expected) == 0;
}

static int parse_device_topic(const char* topic, char* device_type, size_t device_type_size,
    char* device_id, size_t device_id_size, char* suffix, size_t suffix_size)
{
    char parsed_type[MQTT_DEVICE_TYPE_MAX];
    char parsed_id[MQTT_DEVICE_ID_MAX];
    char parsed_suffix[64];

    if (!topic || !device_type || !device_id || !suffix)
        return 0;

    if (sscanf(topic, "device/%31[^/]/%63[^/]/%63[^/]", parsed_type, parsed_id, parsed_suffix) != 3)
        return 0;

    snprintf(device_type, device_type_size, "%s", parsed_type);
    snprintf(device_id, device_id_size, "%s", parsed_id);
    snprintf(suffix, suffix_size, "%s", parsed_suffix);
    return 1;
}

static void mqtt_notify_sensor_event(int plant_id, const char* message)
{
    mqtt_adapter_publish_status(plant_id, message);
}

static void mqtt_record_device_event(int plant_id, const char* event_type, const char* message)
{
    DBRequest req;

    if (plant_id <= 0 || !event_type || !message)
        return;

    event_log_push(&g_event_log, plant_id, event_type, message);
    memset(&req, 0, sizeof(req));
    req.type = DB_REQ_SENSOR;
    req.client_sock = -1;
    snprintf(req.query, DB_QUERY_SIZE,
        "INSERT_EVENT %d %s %s",
        plant_id, event_type, message);
    db_queue_push(&g_db_queue, &req);
}

static void mqtt_forward_bound_device_status(int plant_id, const char* role, const char* payload)
{
    char event_type[64];
    char message[128];
    char status_json[256];
    const char* fallback_event_type;
    const char* fallback_message;

    if (plant_id <= 0 || !role || !payload)
        return;

    memset(event_type, 0, sizeof(event_type));
    memset(message, 0, sizeof(message));

    json_extract_string(payload, "eventType", event_type, sizeof(event_type));
    json_extract_string(payload, "message", message, sizeof(message));

    if (strcmp(role, "water") == 0) {
        fallback_event_type = "DEVICE_WATER_STATUS";
        fallback_message = "Water_device_status";
    } else if (strcmp(role, "arm") == 0) {
        fallback_event_type = "DEVICE_ARM_STATUS";
        fallback_message = "Arm_device_status";
    } else if (strcmp(role, "sensor") == 0) {
        fallback_event_type = "DEVICE_SENSOR_STATUS";
        fallback_message = "Sensor_device_status";
    } else {
        fallback_event_type = "DEVICE_STATUS";
        fallback_message = "Device_status";
    }

    if (event_type[0] == '\0')
        snprintf(event_type, sizeof(event_type), "%s", fallback_event_type);
    if (message[0] == '\0')
        snprintf(message, sizeof(message), "%s", fallback_message);

    snprintf(status_json, sizeof(status_json),
        "{\"eventType\":\"%s\",\"message\":\"%s\"}",
        event_type, message);
    mqtt_adapter_publish_status(plant_id, status_json);
    mqtt_record_device_event(plant_id, event_type, message);
}

static void push_sensor_buffer_record(int plant_id, double temp, double humi, int soil, int light)
{
    Reading r;
    time_t now;
    struct tm tm_now;

    memset(&r, 0, sizeof(r));
    r.plant_id = plant_id;
    r.ts_ms = (uint64_t)time(NULL) * 1000;
    r.temp_c = temp;
    r.humi_pct = humi;
    r.soil_raw = soil;
    r.cds_raw = light;

    now = time(NULL);
    localtime_r(&now, &tm_now);
    strftime(r.created_at, sizeof(r.created_at), "%Y-%m-%d %H:%M:%S", &tm_now);
    sensor_buffer_push(&g_sensor_buffer, &r);
}

static void handle_sensor_publish(MYSQL* conn, int plant_id, const char* payload)
{
    double temp = 0.0;
    double humi = 0.0;
    int soil = 0;
    int light = 0;
    char req[256];
    char out[256];
    char forward[256];

    if (!json_extract_double(payload, "temperature", &temp) &&
        !json_extract_double(payload, "temp", &temp)) {
        return;
    }
    if (!json_extract_double(payload, "humidity", &humi) &&
        !json_extract_double(payload, "humi", &humi)) {
        return;
    }
    if (!json_extract_int(payload, "soilMoisture", &soil) &&
        !json_extract_int(payload, "soil", &soil)) {
        return;
    }
    if (!json_extract_int(payload, "light", &light) &&
        !json_extract_int(payload, "lux", &light)) {
        return;
    }

    snprintf(req, sizeof(req), "POST_SENSOR_DATA %d %.2f %.2f %d %d",
        plant_id, temp, humi, soil, light);

    push_sensor_buffer_record(plant_id, temp, humi, soil, light);
    sensor_event_dispatcher_evaluate(-1, plant_id, temp, humi, soil, light, mqtt_notify_sensor_event);
    sensor_service_post(conn, req, out, sizeof(out));

    snprintf(forward, sizeof(forward),
        "{\"temperature\":%.2f,\"humidity\":%.2f,\"soilMoisture\":%d,\"light\":%d}",
        temp, humi, soil, light);
    mqtt_adapter_publish_sensor(plant_id, forward);
}

static void handle_water_command_publish(MYSQL* conn, int plant_id, const char* payload)
{
    int duration = 0;
    int user_id = 0;
    char login_id[64];
    char result[256];
    char status[320];
    const char* message;

    memset(login_id, 0, sizeof(login_id));

    if (!json_extract_int(payload, "duration", &duration) &&
        !json_extract_int(payload, "seconds", &duration)) {
        mqtt_adapter_publish_status(plant_id, "{\"message\":\"invalid_duration\"}");
        return;
    }

    if (!json_extract_string(payload, "loginId", login_id, sizeof(login_id)) ||
        !user_service_find_user_id_by_login_id(conn, login_id, &user_id)) {
        mqtt_adapter_publish_status(plant_id, "{\"message\":\"login_required\"}");
        return;
    }

    if (plant_service_queue_watering(conn, plant_id, user_id, duration, -1, result, sizeof(result))) {
        snprintf(status, sizeof(status),
            "{\"message\":\"watering_queued\",\"plantId\":%d,\"duration\":%d}",
            plant_id, duration);
    } else {
        message = result;
        if (strncmp(message, "ERROR ", 6) == 0)
            message += 6;
        snprintf(status, sizeof(status),
            "{\"message\":\"%.*s\",\"plantId\":%d}",
            (int)strcspn(message, "\n"),
            message,
            plant_id);
    }

    mqtt_adapter_publish_status(plant_id, status);
}

static int mqtt_extract_required_string(const char* json, const char* key, char* out, size_t out_size)
{
    return json_extract_string(json, key, out, out_size) && out[0] != '\0';
}

static void handle_rpc_publish(MqttClient* client, MYSQL* conn, const char* payload)
{
    char action[64];
    char request_id[64];
    char login_id[64];
    char password[64];
    char name[64];
    char type[64];
    char out[8192];
    int user_id = 0;
    int plant_id = 0;
    int created_plant_id;
    double temp_min = 0.0;
    double temp_max = 0.0;
    double humi_min = 0.0;
    double humi_max = 0.0;
    int soil_min = 0;
    int soil_max = 0;
    int light_min = 0;
    int light_max = 0;

    if (!client || !conn || !payload)
        return;

    if (!mqtt_extract_required_string(payload, "action", action, sizeof(action)) ||
        !mqtt_extract_required_string(payload, "requestId", request_id, sizeof(request_id))) {
        mqtt_publish_rpc_error(client, "", "invalid_request");
        return;
    }

    if (strcmp(action, "login") == 0) {
        if (!mqtt_extract_required_string(payload, "loginId", login_id, sizeof(login_id)) ||
            !mqtt_extract_required_string(payload, "password", password, sizeof(password))) {
            mqtt_publish_rpc_error(client, request_id, "invalid_auth_args");
            return;
        }

        if (!user_service_login(conn, login_id, password, &user_id)) {
            mqtt_publish_rpc_error(client, request_id, "login_failed");
            return;
        }

        snprintf(out, sizeof(out), "{\"user_id\":%d}", user_id);
        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "signup") == 0) {
        if (!mqtt_extract_required_string(payload, "loginId", login_id, sizeof(login_id)) ||
            !mqtt_extract_required_string(payload, "password", password, sizeof(password))) {
            mqtt_publish_rpc_error(client, request_id, "invalid_auth_args");
            return;
        }

        if (!user_service_add(conn, login_id, password, login_id)) {
            mqtt_publish_rpc_error(client, request_id, "signup_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, "{\"message\":\"user_added\"}");
        return;
    }

    if (strcmp(action, "loadPlants") == 0) {
        if (!json_extract_int(payload, "userId", &user_id) || user_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_user_id");
            return;
        }

        if (!plant_service_get_by_user(conn, user_id, out, sizeof(out))) {
            mqtt_publish_rpc_error(client, request_id, "get_plant_by_user_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, mqtt_skip_ok_prefix(out));
        return;
    }

    if (strcmp(action, "getDeviceList") == 0) {
        char device_type[MQTT_DEVICE_TYPE_MAX];

        if (!mqtt_extract_required_string(payload, "deviceType", device_type, sizeof(device_type))) {
            mqtt_publish_rpc_error(client, request_id, "invalid_device_type");
            return;
        }

        if (!mqtt_device_registry_list_live_devices_json(device_type, out, sizeof(out))) {
            mqtt_publish_rpc_error(client, request_id, "device_list_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "addPlant") == 0 || strcmp(action, "editPlant") == 0) {
        if (!json_extract_int(payload, "userId", &user_id) || user_id <= 0 ||
            !mqtt_extract_required_string(payload, "name", name, sizeof(name)) ||
            !mqtt_extract_required_string(payload, "type", type, sizeof(type)) ||
            !json_extract_double(payload, "tempMin", &temp_min) ||
            !json_extract_double(payload, "tempMax", &temp_max) ||
            !json_extract_double(payload, "humiMin", &humi_min) ||
            !json_extract_double(payload, "humiMax", &humi_max) ||
            !json_extract_int(payload, "soilMin", &soil_min) ||
            !json_extract_int(payload, "soilMax", &soil_max) ||
            !json_extract_int(payload, "lightMin", &light_min) ||
            !json_extract_int(payload, "lightMax", &light_max)) {
            mqtt_publish_rpc_error(client, request_id, "invalid_plant_args");
            return;
        }

        if (strcmp(action, "addPlant") == 0) {
            if (!plant_service_add(conn, user_id, name, type,
                    temp_min, temp_max, humi_min, humi_max,
                    soil_min, soil_max, light_min, light_max)) {
                mqtt_publish_rpc_error(client, request_id, "add_plant_failed");
                return;
            }

            created_plant_id = (int)mysql_insert_id(conn);
            if (created_plant_id > 0) {
                plant_owner_cache_set(created_plant_id, user_id);
                plant_threshold_cache_set(
                    created_plant_id,
                    temp_min, temp_max,
                    humi_min, humi_max,
                    soil_min, soil_max,
                    light_min, light_max
                );
            }

            snprintf(out, sizeof(out),
                "{\"message\":\"plant_added\",\"plant_id\":%d}",
                created_plant_id);
            mqtt_publish_rpc_ok_raw(client, request_id, out);
            return;
        }

        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_plant_id");
            return;
        }

        if (!plant_service_edit(conn, plant_id, user_id, name, type,
                temp_min, temp_max, humi_min, humi_max,
                soil_min, soil_max, light_min, light_max)) {
            mqtt_publish_rpc_error(client, request_id, "edit_plant_failed");
            return;
        }

        plant_threshold_cache_set(
            plant_id,
            temp_min, temp_max,
            humi_min, humi_max,
            soil_min, soil_max,
            light_min, light_max
        );
        mqtt_publish_rpc_ok_raw(client, request_id, "{\"message\":\"plant_updated\"}");
        return;
    }

    if (strcmp(action, "deletePlant") == 0) {
        if (!json_extract_int(payload, "userId", &user_id) || user_id <= 0 ||
            !json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_delete_args");
            return;
        }

        if (!plant_service_remove(conn, plant_id, user_id)) {
            mqtt_publish_rpc_error(client, request_id, "remove_plant_failed");
            return;
        }

        plant_owner_cache_remove(plant_id);
        plant_threshold_cache_remove(plant_id);
        mqtt_publish_rpc_ok_raw(client, request_id, "{\"message\":\"plant_removed\"}");
        return;
    }

    if (strcmp(action, "bindDevice") == 0) {
        char role[MQTT_DEVICE_ROLE_MAX];
        char device_type[MQTT_DEVICE_TYPE_MAX];
        char device_id[MQTT_DEVICE_ID_MAX];

        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0 ||
            !mqtt_extract_required_string(payload, "role", role, sizeof(role)) ||
            !mqtt_extract_required_string(payload, "deviceType", device_type, sizeof(device_type)) ||
            !mqtt_extract_required_string(payload, "deviceId", device_id, sizeof(device_id))) {
            mqtt_publish_rpc_error(client, request_id, "invalid_device_binding_args");
            return;
        }

        if (!mqtt_device_registry_bind(plant_id, role, device_type, device_id)) {
            mqtt_publish_rpc_error(client, request_id, "device_binding_failed");
            return;
        }

        snprintf(out, sizeof(out),
            "{\"message\":\"device_bound\",\"plantId\":%d,\"role\":\"%s\",\"deviceType\":\"%s\",\"deviceId\":\"%s\"}",
            plant_id, role, device_type, device_id);
        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "unbindDevice") == 0) {
        char role[MQTT_DEVICE_ROLE_MAX];

        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0 ||
            !mqtt_extract_required_string(payload, "role", role, sizeof(role))) {
            mqtt_publish_rpc_error(client, request_id, "invalid_device_unbind_args");
            return;
        }

        if (!mqtt_device_registry_unbind(plant_id, role)) {
            mqtt_publish_rpc_error(client, request_id, "device_binding_not_found");
            return;
        }

        snprintf(out, sizeof(out),
            "{\"message\":\"device_unbound\",\"plantId\":%d,\"role\":\"%s\"}",
            plant_id, role);
        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "getRecentSensor") == 0) {
        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_plant_id");
            return;
        }

        if (sensor_repository_get_recent_by_plant_json(conn, plant_id, out, sizeof(out)) != 0) {
            mqtt_publish_rpc_error(client, request_id, "recent_sensor_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "getRecentEvent") == 0) {
        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_plant_id");
            return;
        }

        if (event_repository_get_recent_by_plant_json(conn, plant_id, out, sizeof(out)) != 0) {
            mqtt_publish_rpc_error(client, request_id, "recent_event_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "getSensorHistory") == 0) {
        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_plant_id");
            return;
        }

        if (sensor_repository_get_list_by_plant_json(conn, plant_id, out, sizeof(out)) != 0) {
            mqtt_publish_rpc_error(client, request_id, "sensor_history_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    if (strcmp(action, "getEventHistory") == 0) {
        if (!json_extract_int(payload, "plantId", &plant_id) || plant_id <= 0) {
            mqtt_publish_rpc_error(client, request_id, "invalid_plant_id");
            return;
        }

        if (event_repository_get_list_by_plant_json(conn, plant_id, out, sizeof(out)) != 0) {
            mqtt_publish_rpc_error(client, request_id, "event_history_failed");
            return;
        }

        mqtt_publish_rpc_ok_raw(client, request_id, out);
        return;
    }

    mqtt_publish_rpc_error(client, request_id, "unsupported_action");
}

static void handle_publish(MqttClient* client, MYSQL* conn, unsigned char header, unsigned char* body, int body_len)
{
    int qos = (header >> 1) & 0x03;
    int pos = 0;
    int topic_len;
    char topic[128];
    char suffix[64];
    char device_type[MQTT_DEVICE_TYPE_MAX];
    char device_id[MQTT_DEVICE_ID_MAX];
    int plant_id = 0;
    char payload[MQTT_MAX_PACKET];
    MqttDeviceBinding binding;

    if (body_len < 2)
        return;

    topic_len = ((int)body[pos] << 8) | body[pos + 1];
    pos += 2;
    if (topic_len <= 0 || pos + topic_len > body_len || topic_len >= (int)sizeof(topic))
        return;

    memcpy(topic, body + pos, (size_t)topic_len);
    topic[topic_len] = '\0';
    pos += topic_len;

    if (qos > 0) {
        if (pos + 2 > body_len)
            return;
        pos += 2;
    }

    if (body_len - pos >= (int)sizeof(payload))
        return;

    memcpy(payload, body + pos, (size_t)(body_len - pos));
    payload[body_len - pos] = '\0';

    if (mqtt_is_app_request_topic(client, topic)) {
        handle_rpc_publish(client, conn, payload);
        return;
    }

    if (parse_device_topic(topic, device_type, sizeof(device_type), device_id, sizeof(device_id), suffix, sizeof(suffix))) {
        if (strcmp(suffix, "status") == 0) {
            mqtt_device_registry_update_live_device(device_type, device_id, payload);
            if (mqtt_device_registry_find_binding_by_device("sensor", device_type, device_id, &binding))
                mqtt_forward_bound_device_status(binding.plant_id, "sensor", payload);
            if (mqtt_device_registry_find_binding_by_device("water", device_type, device_id, &binding))
                mqtt_forward_bound_device_status(binding.plant_id, "water", payload);
            if (mqtt_device_registry_find_binding_by_device("arm", device_type, device_id, &binding))
                mqtt_forward_bound_device_status(binding.plant_id, "arm", payload);
            return;
        }

        if (strcmp(suffix, "telemetry") == 0 && strcmp(device_type, "sensor") == 0) {
            if (mqtt_device_registry_find_binding_by_device("sensor", device_type, device_id, &binding)) {
                handle_sensor_publish(conn, binding.plant_id, payload);
            }
            return;
        }
    }

    if (!parse_plant_topic(topic, &plant_id, suffix, sizeof(suffix)))
        return;

    if (strcmp(suffix, "sensor") == 0) {
        handle_sensor_publish(conn, plant_id, payload);
    } else if (strcmp(suffix, "command") == 0 && strstr(topic, "/water/") != NULL) {
        handle_water_command_publish(conn, plant_id, payload);
    } else if (strcmp(suffix, "status") == 0) {
        mqtt_adapter_publish_status(plant_id, payload);
    }
}

static int mqtt_send_connack(int sock)
{
    const unsigned char packet[] = {0x20, 0x02, 0x00, 0x00};
    return mqtt_send_all(sock, packet, sizeof(packet));
}

static int mqtt_send_suback(int sock, uint16_t packet_id, int qos)
{
    unsigned char packet[] = {
        0x90, 0x03,
        (unsigned char)((packet_id >> 8) & 0xFF),
        (unsigned char)(packet_id & 0xFF),
        (unsigned char)qos
    };
    return mqtt_send_all(sock, packet, sizeof(packet));
}

static int mqtt_send_unsuback(int sock, uint16_t packet_id)
{
    unsigned char packet[] = {
        0xB0, 0x02,
        (unsigned char)((packet_id >> 8) & 0xFF),
        (unsigned char)(packet_id & 0xFF)
    };
    return mqtt_send_all(sock, packet, sizeof(packet));
}

static int mqtt_send_puback(int sock, uint16_t packet_id)
{
    unsigned char packet[] = {
        0x40, 0x02,
        (unsigned char)((packet_id >> 8) & 0xFF),
        (unsigned char)(packet_id & 0xFF)
    };
    return mqtt_send_all(sock, packet, sizeof(packet));
}

static int mqtt_send_pingresp(int sock)
{
    const unsigned char packet[] = {0xD0, 0x00};
    return mqtt_send_all(sock, packet, sizeof(packet));
}

static void handle_connect_packet(MqttClient* client, unsigned char* body, int body_len)
{
    int pos = 0;
    int protocol_len;
    int client_id_len;

    if (!client || body_len < 12)
        return;

    protocol_len = ((int)body[pos] << 8) | body[pos + 1];
    pos += 2 + protocol_len;
    if (pos + 4 > body_len)
        return;

    pos += 4;
    client_id_len = ((int)body[pos] << 8) | body[pos + 1];
    pos += 2;

    if (client_id_len > 0 && pos + client_id_len <= body_len) {
        size_t copy_len = (size_t)client_id_len;
        if (copy_len >= sizeof(client->client_id))
            copy_len = sizeof(client->client_id) - 1;
        memcpy(client->client_id, body + pos, copy_len);
        client->client_id[copy_len] = '\0';
    }

    mqtt_send_connack(client->sock);
}

static void handle_subscribe_packet(MqttClient* client, unsigned char* body, int body_len)
{
    int pos = 0;
    uint16_t packet_id;

    if (!client || body_len < 5)
        return;

    packet_id = (uint16_t)(((uint16_t)body[pos] << 8) | body[pos + 1]);
    pos += 2;

    pthread_mutex_lock(&g_clients_mutex);
    while (pos + 3 <= body_len) {
        int topic_len = ((int)body[pos] << 8) | body[pos + 1];
        size_t copy_len;

        pos += 2;
        if (topic_len <= 0 || pos + topic_len + 1 > body_len)
            break;

        if (client->subscription_count < MQTT_MAX_SUBS) {
            copy_len = (size_t)topic_len;
            if (copy_len >= sizeof(client->subscriptions[0]))
                copy_len = sizeof(client->subscriptions[0]) - 1;
            memcpy(client->subscriptions[client->subscription_count], body + pos, copy_len);
            client->subscriptions[client->subscription_count][copy_len] = '\0';
            client->subscription_count++;
        }

        pos += topic_len + 1;
    }
    pthread_mutex_unlock(&g_clients_mutex);

    mqtt_send_suback(client->sock, packet_id, 1);
}

static void handle_unsubscribe_packet(MqttClient* client, unsigned char* body, int body_len)
{
    int pos = 0;
    uint16_t packet_id;

    if (!client || body_len < 4)
        return;

    packet_id = (uint16_t)(((uint16_t)body[pos] << 8) | body[pos + 1]);
    pos += 2;

    pthread_mutex_lock(&g_clients_mutex);
    while (pos + 2 <= body_len) {
        int topic_len = ((int)body[pos] << 8) | body[pos + 1];
        char topic[128];
        int i;
        int j;

        pos += 2;
        if (topic_len <= 0 || pos + topic_len > body_len || topic_len >= (int)sizeof(topic))
            break;

        memcpy(topic, body + pos, (size_t)topic_len);
        topic[topic_len] = '\0';
        pos += topic_len;

        for (i = 0; i < client->subscription_count; ++i) {
            if (strcmp(client->subscriptions[i], topic) == 0) {
                for (j = i; j < client->subscription_count - 1; ++j) {
                    snprintf(client->subscriptions[j], sizeof(client->subscriptions[j]), "%s", client->subscriptions[j + 1]);
                }
                client->subscription_count--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);

    mqtt_send_unsuback(client->sock, packet_id);
}

void* mqtt_adapter_thread_main(void* arg)
{
    MYSQL conn;
    int server_sock;
    struct sockaddr_in addr;
    fd_set reads;
    int fd_max;

    (void)arg;

    mysql_init(&conn);
    if (!db_connect(&conn)) {
        fprintf(stderr, "MQTT adapter: db_connect failed\n");
        return NULL;
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("mqtt socket");
        mysql_close(&conn);
        return NULL;
    }

    {
        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(MQTT_PORT);

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("mqtt bind");
        close(server_sock);
        mysql_close(&conn);
        return NULL;
    }

    if (listen(server_sock, 10) < 0) {
        perror("mqtt listen");
        close(server_sock);
        mysql_close(&conn);
        return NULL;
    }

    printf("mqtt adapter listening on %d\n", MQTT_PORT);

    while (1) {
        int i;

        FD_ZERO(&reads);
        FD_SET(server_sock, &reads);
        fd_max = server_sock;

        pthread_mutex_lock(&g_clients_mutex);
        for (i = 0; i < MQTT_MAX_CLIENTS; ++i) {
            if (g_clients[i].in_use) {
                FD_SET(g_clients[i].sock, &reads);
                if (g_clients[i].sock > fd_max)
                    fd_max = g_clients[i].sock;
            }
        }
        pthread_mutex_unlock(&g_clients_mutex);

        if (select(fd_max + 1, &reads, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("mqtt select");
            continue;
        }

        if (FD_ISSET(server_sock, &reads)) {
            int client_sock = accept(server_sock, NULL, NULL);
            if (client_sock >= 0) {
                pthread_mutex_lock(&g_clients_mutex);
                if (!mqtt_alloc_client(client_sock))
                    close(client_sock);
                pthread_mutex_unlock(&g_clients_mutex);
            }
        }

        pthread_mutex_lock(&g_clients_mutex);
        for (i = 0; i < MQTT_MAX_CLIENTS; ++i) {
            unsigned char header;
            int remaining_len;
            unsigned char body[MQTT_MAX_PACKET];
            MqttClient* client = &g_clients[i];

            if (!client->in_use || !FD_ISSET(client->sock, &reads))
                continue;

            if (!recv_full(client->sock, &header, 1) ||
                !mqtt_read_remaining_length(client->sock, &remaining_len) ||
                remaining_len < 0 || remaining_len > MQTT_MAX_PACKET ||
                !recv_full(client->sock, body, remaining_len)) {
                mqtt_close_client(client);
                continue;
            }

            pthread_mutex_unlock(&g_clients_mutex);

            switch (header >> 4) {
                case 1:
                    handle_connect_packet(client, body, remaining_len);
                    break;
                case 3:
                    handle_publish(client, &conn, header, body, remaining_len);
                    if (((header >> 1) & 0x03) > 0) {
                        int topic_len = ((int)body[0] << 8) | body[1];
                        int packet_pos = 2 + topic_len;
                        uint16_t packet_id = (uint16_t)(((uint16_t)body[packet_pos] << 8) | body[packet_pos + 1]);
                        mqtt_send_puback(client->sock, packet_id);
                    }
                    break;
                case 8:
                    handle_subscribe_packet(client, body, remaining_len);
                    break;
                case 10:
                    handle_unsubscribe_packet(client, body, remaining_len);
                    break;
                case 12:
                    mqtt_send_pingresp(client->sock);
                    break;
                case 14:
                    pthread_mutex_lock(&g_clients_mutex);
                    mqtt_close_client(client);
                    pthread_mutex_unlock(&g_clients_mutex);
                    pthread_mutex_lock(&g_clients_mutex);
                    break;
                default:
                    break;
            }

            pthread_mutex_lock(&g_clients_mutex);
        }
        pthread_mutex_unlock(&g_clients_mutex);
    }

    close(server_sock);
    mysql_close(&conn);
    return NULL;
}
