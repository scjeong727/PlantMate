#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <mysql/mysql.h>
#include "plant_service.h"
#include "plant_repository.h"
#include "command_queue.h"
#include "event_service.h"
#include "watering_lock.h"
#include "plant_owner_cache.h"
#include "plant_threshold_cache.h"
extern CommandQueue g_command_queue;

int plant_service_add(MYSQL* conn, int user_id, const char* name, const char* type,
    int has_position_x, double position_x,
    int has_position_y, double position_y,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max)
{
    if (!conn || user_id <= 0 || !name || !type) return 0;
    if (strlen(name) == 0 || strlen(type) == 0) return 0;

    if (temp_min > temp_max) return 0;
    if (humi_min > humi_max) return 0;
    if (soil_min > soil_max) return 0;
    if (light_min > light_max) return 0;

    return plant_repository_add(conn, user_id, name, type,
        has_position_x, position_x,
        has_position_y, position_y,
        temp_min, temp_max,
        humi_min, humi_max,
        soil_min, soil_max,
        light_min, light_max);
}

int plant_service_get_all(MYSQL* conn, char* out, size_t out_size)
{
    if (!conn || !out || out_size == 0) return 0;
    return plant_repository_get_all(conn, out, out_size);
}

int plant_service_get_by_user(MYSQL* conn, int user_id, char* out, size_t out_size)
{
    if (!conn || user_id <= 0 || !out || out_size == 0) return 0;
    return plant_repository_get_by_user(conn, user_id, out, out_size);
}

int plant_service_remove(MYSQL* conn, int plant_id, int user_id)
{
    if (!conn || plant_id <= 0 || user_id <= 0) return 0;

    if (!plant_repository_exists_by_user(conn, plant_id, user_id))
        return 0;

    if (!plant_repository_remove_sensor_data_by_plant(conn, plant_id)) return 0;
    if (!plant_repository_remove_events_by_plant(conn, plant_id)) return 0;

    return plant_repository_remove(conn, plant_id, user_id);
}

int plant_service_edit(MYSQL* conn, int plant_id, int user_id, const char* name, const char* type,
    int has_position_x, double position_x,
    int has_position_y, double position_y,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max)
{
    if (!conn || plant_id <= 0 || user_id <= 0 || !name || !type) return 0;
    if (strlen(name) == 0 || strlen(type) == 0) return 0;

    if (temp_min > temp_max) return 0;
    if (humi_min > humi_max) return 0;
    if (soil_min > soil_max) return 0;
    if (light_min > light_max) return 0;

    if (!plant_repository_exists_by_user(conn, plant_id, user_id))
        return 0;

    return plant_repository_edit(conn, plant_id, user_id, name, type,
        has_position_x, position_x,
        has_position_y, position_y,
        temp_min, temp_max,
        humi_min, humi_max,
        soil_min, soil_max,
        light_min, light_max);
}

int plant_service_queue_watering(MYSQL* conn, int plant_id, int user_id, int duration, int owner_sock, char* out, size_t out_size)
{
    WaterCommand cmd;

    if (!out || out_size == 0) return 0;

    if (plant_id <= 0 || user_id <= 0 || duration <= 0) {
        snprintf(out, out_size, "ERROR invalid_water_args\n");
        return 0;
    }

    if (!plant_repository_exists_by_user(conn, plant_id, user_id)) {
        snprintf(out, out_size, "ERROR plant_not_owned\n");
        return 0;
    }

    if (!watering_try_begin(plant_id)) {
        snprintf(out, out_size, "ERROR watering_in_progress\n");
        return 0;
    }

    cmd.plant_id = plant_id;
    cmd.duration = duration;
    cmd.owner_sock = owner_sock;
    command_queue_push(&g_command_queue, cmd);

    event_service_try_add(conn, plant_id, "WATER_START", "Water_command_queued");

    snprintf(out, out_size, "OK {\"message\":\"watering_queued\"}\n");
    return 1;
}

void handle_add_plant_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    int user_id;
    char name[64], type[64];
    char position_x_text[32], position_y_text[32];
    int has_position_x = 0, has_position_y = 0;
    double position_x = 0, position_y = 0;
    double temp_min, temp_max, humi_min, humi_max;
    int soil_min, soil_max, light_min, light_max;

    if (sscanf(buf,
        "ADD_PLANT %d %63s %63s %31s %31s %lf %lf %lf %lf %d %d %d %d",
        &user_id, name, type, position_x_text, position_y_text,
        &temp_min, &temp_max,
        &humi_min, &humi_max,
        &soil_min, &soil_max,
        &light_min, &light_max) != 13) {
        send(client_sock, "ERROR usage: ADD_PLANT user_id name type position_x position_y temp_min temp_max humi_min humi_max soil_min soil_max light_min light_max\n", 143, 0);
        return;
    }

    if (strcmp(position_x_text, "null") != 0) {
        if (sscanf(position_x_text, "%lf", &position_x) != 1) {
            send(client_sock, "ERROR invalid_position\n", 23, 0);
            return;
        }
        has_position_x = 1;
    }

    if (strcmp(position_y_text, "null") != 0) {
        if (sscanf(position_y_text, "%lf", &position_y) != 1) {
            send(client_sock, "ERROR invalid_position\n", 23, 0);
            return;
        }
        has_position_y = 1;
    }

    if (plant_service_add(conn, user_id, name, type,
        has_position_x, position_x,
        has_position_y, position_y,
        temp_min, temp_max,
        humi_min, humi_max,
        soil_min, soil_max,
        light_min, light_max)) {
        int new_plant_id = (int)mysql_insert_id(conn);

        if (new_plant_id > 0) {
            plant_owner_cache_set(new_plant_id, user_id);
            plant_threshold_cache_set(
                new_plant_id,
                temp_min, temp_max,
                humi_min, humi_max,
                soil_min, soil_max,
                light_min, light_max
            );
        }

        send(client_sock, "OK {\"message\":\"plant_added\"}\n", 31, 0);
    } else {
        send(client_sock, "ERROR add_plant_failed\n", 23, 0);
    }
}

void handle_get_plant_by_user_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    int user_id;
    char out[4096];

    if (sscanf(buf, "GET_PLANT_BY_USER %d", &user_id) != 1) {
        send(client_sock, "ERROR usage: GET_PLANT_BY_USER user_id\n", 40, 0);
        return;
    }

    if (plant_service_get_by_user(conn, user_id, out, sizeof(out)))
        send(client_sock, out, strlen(out), 0);
    else
        send(client_sock, "ERROR get_plant_by_user_failed\n", 31, 0);
}

void handle_get_plant_with_conn(MYSQL* conn, int client_sock)
{
    char out[4096];

    if (plant_service_get_all(conn, out, sizeof(out)))
        send(client_sock, out, strlen(out), 0);
    else
        send(client_sock, "ERROR get_plant_failed\n", 23, 0);
}

void handle_edit_plant_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    int plant_id, user_id;
    char name[64], type[64];
    char position_x_text[32], position_y_text[32];
    int has_position_x = 0, has_position_y = 0;
    double position_x = 0, position_y = 0;
    double temp_min, temp_max, humi_min, humi_max;
    int soil_min, soil_max, light_min, light_max;

    if (sscanf(buf,
        "EDIT_PLANT %d %d %63s %63s %31s %31s %lf %lf %lf %lf %d %d %d %d",
        &plant_id, &user_id, name, type, position_x_text, position_y_text,
        &temp_min, &temp_max,
        &humi_min, &humi_max,
        &soil_min, &soil_max,
        &light_min, &light_max) != 14) {
        send(client_sock, "ERROR usage: EDIT_PLANT plant_id user_id name type position_x position_y temp_min temp_max humi_min humi_max soil_min soil_max light_min light_max\n", 152, 0);
        return;
    }

    if (strcmp(position_x_text, "null") != 0) {
        if (sscanf(position_x_text, "%lf", &position_x) != 1) {
            send(client_sock, "ERROR invalid_position\n", 23, 0);
            return;
        }
        has_position_x = 1;
    }

    if (strcmp(position_y_text, "null") != 0) {
        if (sscanf(position_y_text, "%lf", &position_y) != 1) {
            send(client_sock, "ERROR invalid_position\n", 23, 0);
            return;
        }
        has_position_y = 1;
    }

    if (plant_service_edit(conn, plant_id, user_id, name, type,
        has_position_x, position_x,
        has_position_y, position_y,
        temp_min, temp_max,
        humi_min, humi_max,
        soil_min, soil_max,
        light_min, light_max)) {

        plant_threshold_cache_set(
            plant_id,
            temp_min, temp_max,
            humi_min, humi_max,
            soil_min, soil_max,
            light_min, light_max
        );

        send(client_sock, "OK {\"message\":\"plant_updated\"}\n", 33, 0);
    }
    else {
        send(client_sock, "ERROR edit_plant_failed\n", 24, 0);
    }
}
void handle_remove_plant_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    int plant_id, user_id;

    if (sscanf(buf, "REMOVE_PLANT %d %d", &plant_id, &user_id) != 2) {
        send(client_sock, "ERROR usage: REMOVE_PLANT plant_id user_id\n", 45, 0);
        return;
    }

    if (plant_service_remove(conn, plant_id, user_id)) {
        plant_owner_cache_remove(plant_id);
        plant_threshold_cache_remove(plant_id);
        send(client_sock, "OK {\"message\":\"plant_removed\"}\n", 33, 0);
    } else {
        send(client_sock, "ERROR remove_plant_failed\n", 26, 0);
    }
}

void handle_water_plant_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    int plant_id, user_id, duration;
    char out[128];

    if (sscanf(buf, "WATER_PLANT %d %d %d", &plant_id, &user_id, &duration) != 3) {
        send(client_sock, "ERROR usage: WATER_PLANT plant_id user_id duration\n", 53, 0);
        return;
    }

    if (plant_service_queue_watering(conn, plant_id, user_id, duration, client_sock, out, sizeof(out)))
        send(client_sock, out, strlen(out), 0);
    else
        send(client_sock, out, strlen(out), 0);
}
