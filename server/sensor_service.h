#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#include <mysql/mysql.h>
#include <stddef.h>

int sensor_service_post(MYSQL* conn, const char* req, char* out, size_t out_size);
int sensor_service_get_recent_from_memory(char* out, size_t out_size);
int sensor_service_get_list_by_plant_from_memory(const char* req, char* out, size_t out_size);

void handle_post_sensor_data_with_conn(MYSQL* conn, int client_sock, const char* buf);

#endif
