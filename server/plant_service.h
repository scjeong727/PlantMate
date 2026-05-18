#ifndef PLANT_SERVICE_H
#define PLANT_SERVICE_H

#include <mysql/mysql.h>
#include <stddef.h>
#include "command_queue.h"

int plant_service_add(MYSQL* conn, int user_id, const char* name, const char* type,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max);

int plant_service_get_all(MYSQL* conn, char* out, size_t out_size);
int plant_service_get_by_user(MYSQL* conn, int user_id, char* out, size_t out_size);
int plant_service_remove(MYSQL* conn, int plant_id, int user_id);

int plant_service_edit(MYSQL* conn, int plant_id, int user_id, const char* name, const char* type,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max);
int plant_service_queue_watering(MYSQL* conn, int plant_id, int user_id, int duration, int owner_sock, char* out, size_t out_size);

void handle_add_plant_with_conn(MYSQL* conn, int client_sock, const char* buf);
void handle_get_plant_by_user_with_conn(MYSQL* conn, int client_sock, const char* buf);
void handle_get_plant_with_conn(MYSQL* conn, int client_sock);
void handle_edit_plant_with_conn(MYSQL* conn, int client_sock, const char* buf);
void handle_remove_plant_with_conn(MYSQL* conn, int client_sock, const char* buf);
void handle_water_plant_with_conn(MYSQL* conn, int client_sock, const char* buf);
int plant_repository_get_owner_user_id(MYSQL* conn, int plant_id);

#endif
