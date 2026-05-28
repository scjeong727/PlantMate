#include "mqtt_device_registry.h"

#include "db.h"

#include <mysql/mysql.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static MqttDeviceBinding g_bindings[MQTT_DEVICE_REGISTRY_MAX];
static MqttLiveDevice g_live_devices[MQTT_DEVICE_REGISTRY_MAX];
static pthread_mutex_t g_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

static int mqtt_device_registry_save_binding_locked(int plant_id, const char* role, const char* device_type, const char* device_id)
{
    MYSQL conn;
    char esc_role[MQTT_DEVICE_ROLE_MAX * 2 + 1];
    char esc_type[MQTT_DEVICE_TYPE_MAX * 2 + 1];
    char esc_id[MQTT_DEVICE_ID_MAX * 2 + 1];
    char query[512];

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn))
        return 0;

    mysql_real_escape_string(&conn, esc_role, role, (unsigned long)strlen(role));
    mysql_real_escape_string(&conn, esc_type, device_type, (unsigned long)strlen(device_type));
    mysql_real_escape_string(&conn, esc_id, device_id, (unsigned long)strlen(device_id));
    snprintf(query, sizeof(query),
        "INSERT INTO mqtt_device_bindings (plant_id, role, device_type, device_id) "
        "VALUES (%d, '%s', '%s', '%s') "
        "ON DUPLICATE KEY UPDATE device_type=VALUES(device_type), device_id=VALUES(device_id)",
        plant_id, esc_role, esc_type, esc_id);

    if (mysql_query(&conn, query) != 0) {
        db_close(&conn);
        return 0;
    }

    db_close(&conn);
    return 1;
}

static int mqtt_device_registry_delete_binding_locked(int plant_id, const char* role)
{
    MYSQL conn;
    char esc_role[MQTT_DEVICE_ROLE_MAX * 2 + 1];
    char query[256];

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn))
        return 0;

    mysql_real_escape_string(&conn, esc_role, role, (unsigned long)strlen(role));
    snprintf(query, sizeof(query),
        "DELETE FROM mqtt_device_bindings WHERE plant_id=%d AND role='%s'",
        plant_id, esc_role);

    if (mysql_query(&conn, query) != 0) {
        db_close(&conn);
        return 0;
    }

    db_close(&conn);
    return 1;
}

static int mqtt_device_registry_save_live_device_locked(const char* device_type, const char* device_id, const char* status_payload)
{
    MYSQL conn;
    char esc_type[MQTT_DEVICE_TYPE_MAX * 2 + 1];
    char esc_id[MQTT_DEVICE_ID_MAX * 2 + 1];
    char esc_payload[MQTT_DEVICE_STATUS_MAX * 2 + 1];
    char query[1024];

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn))
        return 0;

    mysql_real_escape_string(&conn, esc_type, device_type, (unsigned long)strlen(device_type));
    mysql_real_escape_string(&conn, esc_id, device_id, (unsigned long)strlen(device_id));
    mysql_real_escape_string(&conn, esc_payload, status_payload ? status_payload : "",
        (unsigned long)strlen(status_payload ? status_payload : ""));

    snprintf(query, sizeof(query),
        "INSERT INTO mqtt_live_devices (device_type, device_id, online, status_payload) "
        "VALUES ('%s', '%s', 1, '%s') "
        "ON DUPLICATE KEY UPDATE online=1, status_payload=VALUES(status_payload), updated_at=CURRENT_TIMESTAMP",
        esc_type, esc_id, esc_payload);

    if (mysql_query(&conn, query) != 0) {
        db_close(&conn);
        return 0;
    }

    db_close(&conn);
    return 1;
}

static int mqtt_device_registry_save_live_device_offline_locked(const char* device_type, const char* device_id)
{
    MYSQL conn;
    char esc_type[MQTT_DEVICE_TYPE_MAX * 2 + 1];
    char esc_id[MQTT_DEVICE_ID_MAX * 2 + 1];
    char query[512];

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn))
        return 0;

    mysql_real_escape_string(&conn, esc_type, device_type, (unsigned long)strlen(device_type));
    mysql_real_escape_string(&conn, esc_id, device_id, (unsigned long)strlen(device_id));
    snprintf(query, sizeof(query),
        "UPDATE mqtt_live_devices SET online=0, updated_at=CURRENT_TIMESTAMP "
        "WHERE device_type='%s' AND device_id='%s'",
        esc_type, esc_id);

    if (mysql_query(&conn, query) != 0) {
        db_close(&conn);
        return 0;
    }

    db_close(&conn);
    return 1;
}

void mqtt_device_registry_init(void)
{
    pthread_mutex_lock(&g_registry_mutex);
    memset(g_bindings, 0, sizeof(g_bindings));
    memset(g_live_devices, 0, sizeof(g_live_devices));
    pthread_mutex_unlock(&g_registry_mutex);
}

int mqtt_device_registry_preload_all(void)
{
    MYSQL conn;
    MYSQL_RES* res;
    MYSQL_ROW row;

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn))
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    memset(g_bindings, 0, sizeof(g_bindings));
    memset(g_live_devices, 0, sizeof(g_live_devices));

    if (mysql_query(&conn,
            "SELECT plant_id, role, device_type, device_id "
            "FROM mqtt_device_bindings ORDER BY id ASC") == 0) {
        int idx = 0;
        res = mysql_store_result(&conn);
        if (res) {
            while ((row = mysql_fetch_row(res)) != NULL && idx < MQTT_DEVICE_REGISTRY_MAX) {
                g_bindings[idx].plant_id = atoi(row[0]);
                snprintf(g_bindings[idx].role, sizeof(g_bindings[idx].role), "%s", row[1] ? row[1] : "");
                snprintf(g_bindings[idx].device_type, sizeof(g_bindings[idx].device_type), "%s", row[2] ? row[2] : "");
                snprintf(g_bindings[idx].device_id, sizeof(g_bindings[idx].device_id), "%s", row[3] ? row[3] : "");
                idx++;
            }
            mysql_free_result(res);
        }
    }

    if (mysql_query(&conn,
            "SELECT device_type, device_id, online, "
            "UNIX_TIMESTAMP(updated_at), status_payload "
            "FROM mqtt_live_devices ORDER BY id ASC") == 0) {
        int idx = 0;
        res = mysql_store_result(&conn);
        if (res) {
            while ((row = mysql_fetch_row(res)) != NULL && idx < MQTT_DEVICE_REGISTRY_MAX) {
                snprintf(g_live_devices[idx].device_type, sizeof(g_live_devices[idx].device_type), "%s", row[0] ? row[0] : "");
                snprintf(g_live_devices[idx].device_id, sizeof(g_live_devices[idx].device_id), "%s", row[1] ? row[1] : "");
                g_live_devices[idx].online = row[2] ? atoi(row[2]) : 0;
                g_live_devices[idx].updated_at = row[3] ? (time_t)atoll(row[3]) : 0;
                snprintf(g_live_devices[idx].status_payload, sizeof(g_live_devices[idx].status_payload), "%s", row[4] ? row[4] : "");
                idx++;
            }
            mysql_free_result(res);
        }
    }

    pthread_mutex_unlock(&g_registry_mutex);
    db_close(&conn);
    return 1;
}

int mqtt_device_registry_bind(int plant_id, const char* role, const char* device_type, const char* device_id)
{
    int i;
    int empty_idx = -1;

    if (plant_id <= 0 || !role || !device_type || !device_id)
        return 0;
    if (role[0] == '\0' || device_type[0] == '\0' || device_id[0] == '\0')
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (g_bindings[i].plant_id == plant_id && strcmp(g_bindings[i].role, role) == 0) {
            snprintf(g_bindings[i].device_type, sizeof(g_bindings[i].device_type), "%s", device_type);
            snprintf(g_bindings[i].device_id, sizeof(g_bindings[i].device_id), "%s", device_id);
            if (!mqtt_device_registry_save_binding_locked(plant_id, role, device_type, device_id)) {
                pthread_mutex_unlock(&g_registry_mutex);
                return 0;
            }
            pthread_mutex_unlock(&g_registry_mutex);
            return 1;
        }
        if (empty_idx < 0 && g_bindings[i].plant_id == 0)
            empty_idx = i;
    }

    if (empty_idx < 0) {
        pthread_mutex_unlock(&g_registry_mutex);
        return 0;
    }

    g_bindings[empty_idx].plant_id = plant_id;
    snprintf(g_bindings[empty_idx].role, sizeof(g_bindings[empty_idx].role), "%s", role);
    snprintf(g_bindings[empty_idx].device_type, sizeof(g_bindings[empty_idx].device_type), "%s", device_type);
    snprintf(g_bindings[empty_idx].device_id, sizeof(g_bindings[empty_idx].device_id), "%s", device_id);
    if (!mqtt_device_registry_save_binding_locked(plant_id, role, device_type, device_id)) {
        memset(&g_bindings[empty_idx], 0, sizeof(g_bindings[empty_idx]));
        pthread_mutex_unlock(&g_registry_mutex);
        return 0;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return 1;
}

int mqtt_device_registry_unbind(int plant_id, const char* role)
{
    int i;

    if (plant_id <= 0 || !role || role[0] == '\0')
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (g_bindings[i].plant_id == plant_id && strcmp(g_bindings[i].role, role) == 0) {
            if (!mqtt_device_registry_delete_binding_locked(plant_id, role)) {
                pthread_mutex_unlock(&g_registry_mutex);
                return 0;
            }
            memset(&g_bindings[i], 0, sizeof(g_bindings[i]));
            pthread_mutex_unlock(&g_registry_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return 0;
}

int mqtt_device_registry_get(int plant_id, const char* role, MqttDeviceBinding* out)
{
    int i;

    if (plant_id <= 0 || !role || role[0] == '\0' || !out)
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (g_bindings[i].plant_id == plant_id && strcmp(g_bindings[i].role, role) == 0) {
            *out = g_bindings[i];
            pthread_mutex_unlock(&g_registry_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return 0;
}

int mqtt_device_registry_find_binding_by_device(const char* role, const char* device_type, const char* device_id, MqttDeviceBinding* out)
{
    int i;

    if (!role || role[0] == '\0' ||
        !device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0' ||
        !out)
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (g_bindings[i].plant_id > 0 &&
            strcmp(g_bindings[i].role, role) == 0 &&
            strcmp(g_bindings[i].device_type, device_type) == 0 &&
            strcmp(g_bindings[i].device_id, device_id) == 0) {
            *out = g_bindings[i];
            pthread_mutex_unlock(&g_registry_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return 0;
}

int mqtt_device_registry_update_live_device(const char* device_type, const char* device_id, const char* status_payload)
{
    int i;
    int empty_idx = -1;
    time_t now;

    if (!device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0')
        return 0;

    now = time(NULL);

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (g_live_devices[i].device_type[0] == '\0' && empty_idx < 0)
            empty_idx = i;

        if (strcmp(g_live_devices[i].device_type, device_type) == 0 &&
            strcmp(g_live_devices[i].device_id, device_id) == 0) {
            g_live_devices[i].online = 1;
            g_live_devices[i].updated_at = now;
            snprintf(g_live_devices[i].status_payload, sizeof(g_live_devices[i].status_payload),
                "%s", status_payload ? status_payload : "");
            if (!mqtt_device_registry_save_live_device_locked(device_type, device_id, status_payload)) {
                pthread_mutex_unlock(&g_registry_mutex);
                return 0;
            }
            pthread_mutex_unlock(&g_registry_mutex);
            return 1;
        }
    }

    if (empty_idx < 0) {
        pthread_mutex_unlock(&g_registry_mutex);
        return 0;
    }

    snprintf(g_live_devices[empty_idx].device_type, sizeof(g_live_devices[empty_idx].device_type), "%s", device_type);
    snprintf(g_live_devices[empty_idx].device_id, sizeof(g_live_devices[empty_idx].device_id), "%s", device_id);
    g_live_devices[empty_idx].online = 1;
    g_live_devices[empty_idx].updated_at = now;
    snprintf(g_live_devices[empty_idx].status_payload, sizeof(g_live_devices[empty_idx].status_payload),
        "%s", status_payload ? status_payload : "");
    if (!mqtt_device_registry_save_live_device_locked(device_type, device_id, status_payload)) {
        memset(&g_live_devices[empty_idx], 0, sizeof(g_live_devices[empty_idx]));
        pthread_mutex_unlock(&g_registry_mutex);
        return 0;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return 1;
}

int mqtt_device_registry_collect_bound_devices(const char* role, MqttDeviceIdentity* out, int max_count)
{
    int i;
    int j;
    int count = 0;

    if (!out || max_count <= 0)
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX && count < max_count; ++i) {
        if (g_bindings[i].plant_id <= 0 ||
            g_bindings[i].device_type[0] == '\0' ||
            g_bindings[i].device_id[0] == '\0')
            continue;
        if (role && role[0] != '\0' && strcmp(g_bindings[i].role, role) != 0)
            continue;

        for (j = 0; j < count; ++j) {
            if (strcmp(out[j].device_type, g_bindings[i].device_type) == 0 &&
                strcmp(out[j].device_id, g_bindings[i].device_id) == 0)
                break;
        }
        if (j < count)
            continue;

        snprintf(out[count].device_type, sizeof(out[count].device_type), "%s", g_bindings[i].device_type);
        snprintf(out[count].device_id, sizeof(out[count].device_id), "%s", g_bindings[i].device_id);
        count++;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return count;
}

int mqtt_device_registry_is_live_device_online(const char* device_type, const char* device_id, int timeout_seconds)
{
    int i;
    int online = 0;
    time_t now = time(NULL);

    if (!device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0')
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (strcmp(g_live_devices[i].device_type, device_type) == 0 &&
            strcmp(g_live_devices[i].device_id, device_id) == 0) {
            online = g_live_devices[i].online &&
                (timeout_seconds <= 0 || now - g_live_devices[i].updated_at <= timeout_seconds);
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return online;
}

int mqtt_device_registry_mark_live_device_offline(const char* device_type, const char* device_id)
{
    int i;
    int found = 0;

    if (!device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0')
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (strcmp(g_live_devices[i].device_type, device_type) == 0 &&
            strcmp(g_live_devices[i].device_id, device_id) == 0) {
            g_live_devices[i].online = 0;
            g_live_devices[i].updated_at = time(NULL);
            found = 1;
            break;
        }
    }

    if (found && !mqtt_device_registry_save_live_device_offline_locked(device_type, device_id)) {
        pthread_mutex_unlock(&g_registry_mutex);
        return 0;
    }

    pthread_mutex_unlock(&g_registry_mutex);
    return found;
}

int mqtt_device_registry_mark_stale_live_devices_offline(int timeout_seconds)
{
    int i;
    int count = 0;
    time_t now = time(NULL);

    if (timeout_seconds <= 0)
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX; ++i) {
        if (!g_live_devices[i].online ||
            g_live_devices[i].device_type[0] == '\0' ||
            g_live_devices[i].device_id[0] == '\0')
            continue;
        if (now - g_live_devices[i].updated_at <= timeout_seconds)
            continue;

        g_live_devices[i].online = 0;
        g_live_devices[i].updated_at = now;
        mqtt_device_registry_save_live_device_offline_locked(
            g_live_devices[i].device_type,
            g_live_devices[i].device_id);
        count++;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return count;
}

int mqtt_device_registry_list_live_devices_json(const char* device_type, char* out, size_t out_size)
{
    int i;
    size_t len = 0;
    int first = 1;

    if (!device_type || device_type[0] == '\0' || !out || out_size == 0)
        return 0;

    pthread_mutex_lock(&g_registry_mutex);
    len += snprintf(out + len, out_size - len, "[");
    for (i = 0; i < MQTT_DEVICE_REGISTRY_MAX && len < out_size; ++i) {
        if (!g_live_devices[i].online)
            continue;
        if (strcmp(g_live_devices[i].device_type, device_type) != 0)
            continue;

        len += snprintf(out + len, out_size - len,
            "%s\"%s\"",
            first ? "" : ",",
            g_live_devices[i].device_id);
        first = 0;
    }
    snprintf(out + len, out_size - len, "]");
    pthread_mutex_unlock(&g_registry_mutex);
    return 1;
}
