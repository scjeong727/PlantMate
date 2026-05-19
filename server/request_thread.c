#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>

#include "session_manager.h"
#include "db_queue.h"
#include "request_thread.h"
#include "command_queue.h"
#include "sensor_buffer.h"
#include "event_log.h"
#include "plant_owner_cache.h"
#include "device_lock.h"
#include "ros2_bridge.h"
#include "mqtt_adapter.h"
#include "mqtt_device_registry.h"

#define PORT 9000
#define BUF_SIZE 4096

extern CommandQueue g_command_queue;
extern DBQueue g_db_queue;
extern SensorBuffer g_sensor_buffer;
extern EventLog g_event_log;
extern DeviceLock g_sensor_device_lock;
extern DeviceLock g_water_device_lock;

static int is_device_globally_available(const char* device_path)
{
    if (!device_path || device_path[0] == '\0')
        return 0;

    if (!device_lock_is_available(&g_sensor_device_lock, device_path))
        return 0;

    if (!device_lock_is_available(&g_water_device_lock, device_path))
        return 0;

    return 1;
}


static void send_water_device_list(int client_sock)
{
    DIR* dir;
    struct dirent* ent;
    char out[2048];
    int first = 1;

    dir = opendir("/dev");
    if (!dir) {
        send(client_sock, "ERROR cannot_open_dev\n", 22, 0);
        return;
    }

    strcpy(out, "OK [");

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "ttyACM", 6) == 0 ||
            strncmp(ent->d_name, "ttyUSB", 6) == 0) {
            char path[256];
            char item[320];

            if (snprintf(path, sizeof(path), "/dev/%s", ent->d_name) >= (int)sizeof(path))
                continue;

            if (!is_device_globally_available(path))
                continue;

            snprintf(item, sizeof(item), "%s\"%s\"", first ? "" : ",", path);

            if (strlen(out) + strlen(item) + 2 < sizeof(out)) {
                strcat(out, item);
                first = 0;
            }
        }
    }

    closedir(dir);
    strcat(out, "]\n");
    send(client_sock, out, strlen(out), 0);
}


static void send_sensor_device_list(int client_sock)
{
    DIR* dir;
    struct dirent* ent;
    char out[2048];
    int first = 1;

    dir = opendir("/dev");
    if (!dir) {
        send(client_sock, "ERROR cannot_open_dev\n", 22, 0);
        return;
    }

    strcpy(out, "OK [");

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "ttyACM", 6) == 0 ||
            strncmp(ent->d_name, "ttyUSB", 6) == 0) {
            char path[256];
            char item[320];

            if (snprintf(path, sizeof(path), "/dev/%s", ent->d_name) >= (int)sizeof(path))
                continue;

            if (!is_device_globally_available(path))
                continue;

            snprintf(item, sizeof(item), "%s\"%s\"", first ? "" : ",", path);

            if (strlen(out) + strlen(item) + 2 < sizeof(out)) {
                strcat(out, item);
                first = 0;
            }
        }
    }

    closedir(dir);
    strcat(out, "]\n");
    send(client_sock, out, strlen(out), 0);
}


static int handle_water_device_command(int client_sock, const char* buf)
{
    char device_path[256];

    if (strncmp(buf, "GET_WATER_DEVICE_LIST", 21) == 0) {
        send_water_device_list(client_sock);
        return 1;
    }

    if (sscanf(buf, "SET_WATER_DEVICE %255s", device_path) == 1) {
        if (strncmp(device_path, "/dev/ttyACM", 11) != 0 &&
            strncmp(device_path, "/dev/ttyUSB", 11) != 0 &&
            strncmp(device_path, "/tmp/ttyACM", 11) != 0) {
            send(client_sock, "ERROR invalid_device\n", 21, 0);
            return 1;
        }

        if (access(device_path, F_OK) != 0) {
            send(client_sock, "ERROR device_not_found\n", 23, 0);
            return 1;
        }

        if (!is_device_globally_available(device_path)) {
            send(client_sock, "ERROR device_busy\n", 18, 0);
            return 1;
        }

        if (!device_lock_acquire(&g_water_device_lock, device_path, client_sock)) {
            send(client_sock, "ERROR device_busy\n", 18, 0);
            return 1;
        }

        send(client_sock, "OK {\"message\":\"water_device_set\"}\n", 36, 0);
        return 1;
    }

    return 0;
}


static int require_owned_plant(int client_sock, int plant_id)
{
    int user_id = session_get_user_id_by_sock(client_sock);

    if (user_id <= 0)
        return 0;

    return plant_owner_cache_exists_by_user(plant_id, user_id);
}

static int is_safe_robot_action(const char* action)
{
    size_t i;

    if (!action || action[0] == '\0')
        return 0;

    for (i = 0; action[i] != '\0'; ++i) {
        if (!isalnum((unsigned char)action[i]) &&
            action[i] != '_' &&
            action[i] != '-')
            return 0;
    }

    return 1;
}

static void copy_json_string(char* out, size_t out_size, const char* in)
{
    size_t pos = 0;
    size_t i;

    if (!out || out_size == 0)
        return;

    out[0] = '\0';
    if (!in)
        return;

    for (i = 0; in[i] != '\0' && pos + 2 < out_size; ++i) {
        if (in[i] == '"' || in[i] == '\\') {
            if (pos + 3 >= out_size)
                break;
            out[pos++] = '\\';
            out[pos++] = in[i];
        } else if ((unsigned char)in[i] < 32) {
            out[pos++] = ' ';
        } else {
            out[pos++] = in[i];
        }
    }

    out[pos] = '\0';
}


static void handle_cache_query(int client_sock, const char* buf)
{
    char out[16384];
    int plant_id, limit;

    memset(out, 0, sizeof(out));

    if (sscanf(buf, "GET_RECENT_SENSOR_BY_PLANT %d", &plant_id) == 1) {
        if (!require_owned_plant(client_sock, plant_id)) {
            send(client_sock, "ERROR plant_not_owned\n", 22, 0);
            return;
        }
        sensor_buffer_get_recent_by_plant_json(&g_sensor_buffer, plant_id, out, sizeof(out));
        send(client_sock, out, strlen(out), 0);
        return;
    }

    if (sscanf(buf, "GET_RECENT_EVENT_BY_PLANT %d", &plant_id) == 1) {
        if (!require_owned_plant(client_sock, plant_id)) {
            send(client_sock, "ERROR plant_not_owned\n", 22, 0);
            return;
        }
        event_log_get_recent_by_plant_json(&g_event_log, plant_id, out, sizeof(out));
        send(client_sock, out, strlen(out), 0);
        return;
    }

    if (sscanf(buf, "GET_SENSOR_LIST_BY_PLANT %d %d", &plant_id, &limit) == 2) {
        if (!require_owned_plant(client_sock, plant_id)) {
            send(client_sock, "ERROR plant_not_owned\n", 22, 0);
            return;
        }
        sensor_buffer_get_list_by_plant_json(&g_sensor_buffer, plant_id, limit, out, sizeof(out));
        send(client_sock, out, strlen(out), 0);
        return;
    }

    if (sscanf(buf, "GET_EVENT_LIST_BY_PLANT %d %d", &plant_id, &limit) == 2) {
        if (!require_owned_plant(client_sock, plant_id)) {
            send(client_sock, "ERROR plant_not_owned\n", 22, 0);
            return;
        }
        event_log_get_list_by_plant_json(&g_event_log, plant_id, limit, out, sizeof(out));
        send(client_sock, out, strlen(out), 0);
        return;
    }

    if (strncmp(buf, "GET_RECENT_SENSOR", 17) == 0) {
        send(client_sock, "ERROR deprecated_api\n", 21, 0);
        return;
    }

    if (strncmp(buf, "GET_RECENT_EVENT", 16) == 0) {
        send(client_sock, "ERROR deprecated_api\n", 21, 0);
        return;
    }

    send(client_sock, "ERROR invalid_cache_query\n", 26, 0);
}


static int rewrite_plant_command_with_session_user(int client_sock, const char* in, char* out, size_t out_size)
{
    int user_id;
    int ignored_user_id;
    int plant_id;
    char name[64];
    char type[64];

    double temp_min, temp_max;
    double humi_min, humi_max;
    int soil_min, soil_max;
    int light_min, light_max;

    if (!in || !out || out_size == 0)
        return 0;

    user_id = session_get_user_id_by_sock(client_sock);
    if (user_id <= 0) {
        snprintf(out, out_size, "ERROR not_logged_in\n");
        return -1;
    }

    if (strncmp(in, "ADD_PLANT ", 10) == 0) {
        if (sscanf(in,
            "ADD_PLANT %d %63s %63s %lf %lf %lf %lf %d %d %d %d",
            &ignored_user_id, name, type,
            &temp_min, &temp_max,
            &humi_min, &humi_max,
            &soil_min, &soil_max,
            &light_min, &light_max) == 11)
        {
            snprintf(out, out_size,
                "ADD_PLANT %d %s %s %.2f %.2f %.2f %.2f %d %d %d %d",
                user_id, name, type,
                temp_min, temp_max,
                humi_min, humi_max,
                soil_min, soil_max,
                light_min, light_max);
            return 1;
        }

        if (sscanf(in,
            "ADD_PLANT %63s %63s %lf %lf %lf %lf %d %d %d %d",
            name, type,
            &temp_min, &temp_max,
            &humi_min, &humi_max,
            &soil_min, &soil_max,
            &light_min, &light_max) == 10)
        {
            snprintf(out, out_size,
                "ADD_PLANT %d %s %s %.2f %.2f %.2f %.2f %d %d %d %d",
                user_id, name, type,
                temp_min, temp_max,
                humi_min, humi_max,
                soil_min, soil_max,
                light_min, light_max);
            return 1;
        }

        snprintf(out, out_size,
            "ERROR usage: ADD_PLANT name type temp_min temp_max humi_min humi_max soil_min soil_max light_min light_max\n");
        return -1;
    }

    if (strncmp(in, "GET_PLANT_BY_USER", 17) == 0) {
        snprintf(out, out_size, "GET_PLANT_BY_USER %d", user_id);
        return 1;
    }

    if (strncmp(in, "REMOVE_PLANT ", 13) == 0) {
        if (sscanf(in, "REMOVE_PLANT %d %d", &plant_id, &ignored_user_id) >= 1 ||
            sscanf(in, "REMOVE_PLANT %d", &plant_id) == 1) {
            snprintf(out, out_size, "REMOVE_PLANT %d %d", plant_id, user_id);
            return 1;
        }
        snprintf(out, out_size, "ERROR usage: REMOVE_PLANT plant_id\n");
        return -1;
    }

    if (strncmp(in, "EDIT_PLANT ", 11) == 0) {
        if (sscanf(in,
            "EDIT_PLANT %d %d %63s %63s %lf %lf %lf %lf %d %d %d %d",
            &plant_id, &ignored_user_id, name, type,
            &temp_min, &temp_max,
            &humi_min, &humi_max,
            &soil_min, &soil_max,
            &light_min, &light_max) == 12)
        {
            snprintf(out, out_size,
                "EDIT_PLANT %d %d %s %s %.2f %.2f %.2f %.2f %d %d %d %d",
                plant_id, user_id, name, type,
                temp_min, temp_max,
                humi_min, humi_max,
                soil_min, soil_max,
                light_min, light_max);
            return 1;
        }

        if (sscanf(in,
            "EDIT_PLANT %d %63s %63s %lf %lf %lf %lf %d %d %d %d",
            &plant_id, name, type,
            &temp_min, &temp_max,
            &humi_min, &humi_max,
            &soil_min, &soil_max,
            &light_min, &light_max) == 11)
        {
            snprintf(out, out_size,
                "EDIT_PLANT %d %d %s %s %.2f %.2f %.2f %.2f %d %d %d %d",
                plant_id, user_id, name, type,
                temp_min, temp_max,
                humi_min, humi_max,
                soil_min, soil_max,
                light_min, light_max);
            return 1;
        }

        snprintf(out, out_size,
            "ERROR usage: EDIT_PLANT plant_id name type temp_min temp_max humi_min humi_max soil_min soil_max light_min light_max\n");
        return -1;
    }

    return 0;
}


static int rewrite_water_command_with_session_user(int client_sock, const char* in, char* out, size_t out_size)
{
    int user_id;
    int plant_id;
    int duration;

    if (!in || !out || out_size == 0)
        return 0;

    user_id = session_get_user_id_by_sock(client_sock);
    if (user_id <= 0) {
        snprintf(out, out_size, "ERROR not_logged_in\n");
        return -1;
    }

    if (sscanf(in, "WATER_PLANT %d %d", &plant_id, &duration) != 2) {
        snprintf(out, out_size, "ERROR usage: WATER_PLANT plant_id duration\n");
        return -1;
    }

    if (plant_id <= 0 || duration <= 0) {
        snprintf(out, out_size, "ERROR invalid_water_args\n");
        return -1;
    }

    snprintf(out, out_size, "WATER_PLANT %d %d %d", plant_id, user_id, duration);
    return 1;
}


static void handle_sensor_device_command(int client_sock, const char* buf)
{
    char device_path[256];

    if (strncmp(buf, "GET_SENSOR_DEVICE_LIST", 22) == 0) {
        send_sensor_device_list(client_sock);
        return;
    }

    if (sscanf(buf, "SET_SENSOR_DEVICE %255s", device_path) == 1) {
        if (strncmp(device_path, "/dev/ttyACM", 11) != 0 &&
            strncmp(device_path, "/dev/ttyUSB", 11) != 0 &&
            strncmp(device_path, "/tmp/ttyACM", 11) != 0) {
            send(client_sock, "ERROR invalid_device\n", 21, 0);
            return;
        }

        if (access(device_path, F_OK) != 0) {
            send(client_sock, "ERROR device_not_found\n", 23, 0);
            return;
        }

        if (!is_device_globally_available(device_path)) {
            send(client_sock, "ERROR device_busy\n", 18, 0);
            return;
        }

        if (!device_lock_acquire(&g_sensor_device_lock, device_path, client_sock)) {
            send(client_sock, "ERROR device_busy\n", 18, 0);
            return;
        }

        send(client_sock, "OK {\"message\":\"sensor_device_set\"}\n", 37, 0);
        return;
    }

    send(client_sock, "ERROR invalid_sensor_device_command\n", 36, 0);
}


static void handle_sensor_stream_command(int client_sock, const char* buf)
{
    int plant_id;

    if (strncmp(buf, "START_SENSOR_STREAM ", 20) == 0) {
        if (sscanf(buf, "START_SENSOR_STREAM %d", &plant_id) != 1 || plant_id <= 0) {
            send(client_sock, "ERROR usage: START_SENSOR_STREAM plant_id\n", 42, 0);
            return;
        }

        if (!require_owned_plant(client_sock, plant_id)) {
            send(client_sock, "ERROR plant_not_owned\n", 22, 0);
            return;
        }

        if (!device_lock_enable_sensor_stream(&g_sensor_device_lock, client_sock, plant_id)) {
            send(client_sock, "ERROR sensor_device_not_selected\n", 33, 0);
            return;
        }

        send(client_sock, "OK {\"message\":\"sensor_stream_started\"}\n", 40, 0);
        return;
    }

    if (strncmp(buf, "STOP_SENSOR_STREAM", 18) == 0) {
        if (!device_lock_disable_sensor_stream(&g_sensor_device_lock, client_sock)) {
            send(client_sock, "ERROR not_sensor_owner\n", 23, 0);
            return;
        }

        send(client_sock, "OK {\"message\":\"sensor_stream_stopped\"}\n", 40, 0);
        return;
    }

    send(client_sock, "ERROR invalid_sensor_stream_command\n", 36, 0);
}

static void handle_robot_command(int client_sock, const char* buf)
{
    int plant_id;
    char action[ROS2_BRIDGE_ACTION_MAX];
    char detail[ROS2_BRIDGE_DETAIL_MAX];
    char escaped_detail[ROS2_BRIDGE_DETAIL_MAX * 2];
    char payload[512];
    MqttDeviceBinding mqtt_binding;

    memset(action, 0, sizeof(action));
    memset(detail, 0, sizeof(detail));

    if (sscanf(buf, "ROBOT_COMMAND %d %63s %255[^\n]", &plant_id, action, detail) < 2) {
        send(client_sock, "ERROR usage: ROBOT_COMMAND plant_id action [detail]\n", 51, 0);
        return;
    }

    if (!is_safe_robot_action(action)) {
        send(client_sock, "ERROR invalid_robot_action\n", 27, 0);
        return;
    }

    if (!require_owned_plant(client_sock, plant_id)) {
        send(client_sock, "ERROR plant_not_owned\n", 22, 0);
        return;
    }

    if (mqtt_device_registry_get(plant_id, "robot", &mqtt_binding)) {
        copy_json_string(escaped_detail, sizeof(escaped_detail), detail);
        snprintf(payload, sizeof(payload),
            "{\"plantId\":%d,\"action\":\"%s\",\"detail\":\"%s\"}",
            plant_id, action, escaped_detail);
        mqtt_adapter_publish_device_command(
            mqtt_binding.device_type,
            mqtt_binding.device_id,
            action,
            payload
        );
        printf("robot command delegated to mqtt device: type=%s id=%s action=%s\n",
            mqtt_binding.device_type, mqtt_binding.device_id, action);
        send(client_sock, "OK {\"message\":\"robot_command_published\"}\n", 42, 0);
        return;
    }

    if (ros2_bridge_publish_command(plant_id, action, detail)) {
        send(client_sock, "OK {\"message\":\"robot_command_published\"}\n", 42, 0);
        return;
    }

    send(client_sock, "ERROR robot_command_publish_failed\n", 35, 0);
}


void* request_thread_main(void* arg)
{
    (void)arg;

    int server_sock, client_sock = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    fd_set reads, cpy_reads;
    int fd_max;
    char buf[BUF_SIZE];

    signal(SIGPIPE, SIG_IGN);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        return NULL;
    }

    {
        int opt = 1;
        if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            perror("setsockopt");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_sock);
        return NULL;
    }

    if (listen(server_sock, 10) == -1) {
        perror("listen");
        close(server_sock);
        return NULL;
    }

    FD_ZERO(&reads);
    FD_SET(server_sock, &reads);
    fd_max = server_sock;

    printf("request thread listening on %d\n", PORT);

    while (1)
    {
        cpy_reads = reads;

        if (select(fd_max + 1, &cpy_reads, 0, 0, 0) == -1) {
            perror("select");
            continue;
        }

        for (int i = 0; i <= fd_max; i++)
        {
            if (!FD_ISSET(i, &cpy_reads))
                continue;

            if (i == server_sock)
            {
                client_len = sizeof(client_addr);
                client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
                if (client_sock == -1) {
                    perror("accept");
                    continue;
                }

                FD_SET(client_sock, &reads);
                if (client_sock > fd_max)
                    fd_max = client_sock;

                printf("client connected\n");
            }
            else
            {
                int len = recv(i, buf, BUF_SIZE - 1, 0);

                if (len <= 0)
                {
                    if (len < 0)
                        perror("recv");

                    session_remove_by_sock(i);
                    device_lock_release_by_owner(&g_sensor_device_lock, i);
                    device_lock_release_by_owner(&g_water_device_lock, i);
                    FD_CLR(i, &reads);
                    close(i);
                    printf("client disconnected\n");
                    continue;
                }

                buf[len] = '\0';
                if (handle_water_device_command(i, buf)) {
                    continue;
                }
                
                if (strncmp(buf, "GET_RECENT_SENSOR_BY_PLANT", 26) == 0 ||
                    strncmp(buf, "GET_RECENT_EVENT_BY_PLANT", 25) == 0 ||
                    strncmp(buf, "GET_SENSOR_LIST_BY_PLANT", 24) == 0 ||
                    strncmp(buf, "GET_EVENT_LIST_BY_PLANT", 23) == 0 ||
                    strncmp(buf, "GET_RECENT_SENSOR", 17) == 0 ||
                    strncmp(buf, "GET_RECENT_EVENT", 16) == 0)
                {
                    handle_cache_query(i, buf);
                }
                else if (strncmp(buf, "GET_SENSOR_LIST", 15) == 0 ||
                         strncmp(buf, "GET_EVENT_LIST", 14) == 0)
                {
                    send(i, "ERROR deprecated_api\n", 21, 0);
                }
                else if (strncmp(buf, "GET_SENSOR_DEVICE_LIST", 22) == 0 ||
                         strncmp(buf, "SET_SENSOR_DEVICE ", 18) == 0)
                {
                    handle_sensor_device_command(i, buf);
                }
                
                else if (strncmp(buf, "START_SENSOR_STREAM ", 20) == 0 ||
                        strncmp(buf, "STOP_SENSOR_STREAM", 18) == 0)
                {
                    handle_sensor_stream_command(i, buf);
                }
                else if (strncmp(buf, "ROBOT_COMMAND ", 14) == 0)
                {
                    handle_robot_command(i, buf);
                }

                else if (strncmp(buf, "WATER_PLANT ", 12) == 0)
                {
                    DBRequest req;
                    char rewritten[DB_QUERY_SIZE];
                    int rewrite_result;

                    rewrite_result = rewrite_water_command_with_session_user(i, buf, rewritten, sizeof(rewritten));

                    if (rewrite_result < 0) {
                        send(i, rewritten, strlen(rewritten), 0);
                        continue;
                    }

                    memset(&req, 0, sizeof(req));
                    req.type = DB_REQ_CLIENT;
                    req.client_sock = i;
                    snprintf(req.query, DB_QUERY_SIZE, "%s", rewritten);
                    db_queue_push(&g_db_queue, &req);
                }
                else if (strncmp(buf, "LOGIN ", 6) == 0)
                {
                    char login_id[64], password[64];
                    DBRequest req;

                    if (sscanf(buf, "LOGIN %63s %63s", login_id, password) != 2) {
                        send(i, "ERROR invalid_login_format\n", 27, 0);
                        continue;
                    }

                    if (session_is_login_active(login_id)) {
                        send(i, "ERROR already_logged_in\n", 24, 0);
                        continue;
                    }

                    memset(&req, 0, sizeof(req));
                    req.type = DB_REQ_CLIENT;
                    req.client_sock = i;
                    snprintf(req.query, DB_QUERY_SIZE, "%s", buf);
                    db_queue_push(&g_db_queue, &req);
                }
                else if (strncmp(buf, "ADD_PLANT ", 10) == 0 ||
                         strncmp(buf, "GET_PLANT_BY_USER", 17) == 0 ||
                         strncmp(buf, "REMOVE_PLANT ", 13) == 0 ||
                         strncmp(buf, "EDIT_PLANT ", 11) == 0)
                {
                    DBRequest req;
                    char rewritten[DB_QUERY_SIZE];
                    int rewrite_result;

                    rewrite_result = rewrite_plant_command_with_session_user(i, buf, rewritten, sizeof(rewritten));

                    if (rewrite_result < 0) {
                        send(i, rewritten, strlen(rewritten), 0);
                        continue;
                    }

                    if (rewrite_result == 1) {
                        memset(&req, 0, sizeof(req));
                        req.type = DB_REQ_CLIENT;
                        req.client_sock = i;
                        snprintf(req.query, DB_QUERY_SIZE, "%s", rewritten);
                        db_queue_push(&g_db_queue, &req);
                        continue;
                    }

                    send(i, "ERROR invalid_plant_command\n", 28, 0);
                }
                else
                {
                    DBRequest req;

                    memset(&req, 0, sizeof(req));
                    req.type = DB_REQ_CLIENT;
                    req.client_sock = i;
                    snprintf(req.query, DB_QUERY_SIZE, "%s", buf);
                    db_queue_push(&g_db_queue, &req);
                }
            }
        }
    }

    close(server_sock);
    return NULL;
}
