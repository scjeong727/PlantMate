#include "sensor_service.h"
#include "sensor_repository.h"
#include "sensor_buffer.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

extern SensorBuffer g_sensor_buffer;

int sensor_service_post(MYSQL* conn, const char* req, char* out, size_t out_size)
{
    int plant_id, soil, light;
    double temp, humi;

    if (!conn || !req || !out || out_size == 0) return -1;

    if (sscanf(req, "POST_SENSOR_DATA %d %lf %lf %d %d",
               &plant_id, &temp, &humi, &soil, &light) != 5) {
        snprintf(out, out_size, "ERROR {\"message\":\"bad_request\"}");
        return -1;
    }

    if (sensor_repo_add(conn, plant_id, temp, humi, soil, light) != 0) {
        snprintf(out, out_size, "ERROR {\"message\":\"sensor_add_failed\"}");
        return -1;
    }

    snprintf(out, out_size, "OK {\"message\":\"sensor_saved\"}");
    return 0;
}

int sensor_service_get_recent_from_memory(char* out, size_t out_size)
{
    Reading r;

    if (!out || out_size == 0) return -1;

    if (!sensor_buffer_get_recent(&g_sensor_buffer, 0, &r)) {
        snprintf(out, out_size, "OK []");
        return 0;
    }

    snprintf(out, out_size,
        "OK [{\"id\":%d,\"plant_id\":%d,\"temp\":%.2f,\"humi\":%.2f,\"soil\":%d,\"light\":%d,\"created_at\":\"%s\"}]",
        r.id, r.plant_id, r.temp_c, r.humi_pct, r.soil_raw, r.cds_raw, r.created_at);
    return 0;
}

int sensor_service_get_list_by_plant_from_memory(const char* req, char* out, size_t out_size)
{
    int plant_id, limit;
    Reading items[64];
    int n, i;
    size_t len = 0;

    if (!req || !out || out_size == 0) return -1;

    if (sscanf(req, "GET_SENSOR_LIST_BY_PLANT %d %d", &plant_id, &limit) != 2) {
        snprintf(out, out_size, "ERROR usage: GET_SENSOR_LIST_BY_PLANT plant_id limit");
        return -1;
    }

    if (limit < 1) limit = 1;
    if (limit > 64) limit = 64;

    n = sensor_buffer_get_recent_list_by_plant(&g_sensor_buffer, plant_id, items, limit);

    len += snprintf(out + len, out_size - len, "OK [");
    for (i = 0; i < n && len < out_size; ++i) {
        len += snprintf(out + len, out_size - len,
            "%s{\"id\":%d,\"plant_id\":%d,\"temp\":%.2f,\"humi\":%.2f,\"soil\":%d,\"light\":%d,\"created_at\":\"%s\"}",
            (i == 0 ? "" : ","),
            items[i].id, items[i].plant_id, items[i].temp_c, items[i].humi_pct,
            items[i].soil_raw, items[i].cds_raw, items[i].created_at);
    }
    snprintf(out + len, out_size - len, "]");
    return 0;
}

void handle_post_sensor_data_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    char out[256];
    sensor_service_post(conn, buf, out, sizeof(out));
    if (client_sock >= 0)
        send(client_sock, out, strlen(out), 0);
}
