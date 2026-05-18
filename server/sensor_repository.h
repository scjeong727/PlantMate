#ifndef SENSOR_REPOSITORY_H
#define SENSOR_REPOSITORY_H

#include <mysql/mysql.h>
#include <stddef.h>

int sensor_repo_add(MYSQL* conn, int plant_id, double temp, double humi, int soil, int light);
int sensor_repo_get_recent_json(MYSQL* conn, char* out, size_t out_size);
int sensor_repo_get_list_json(MYSQL* conn, char* out, size_t out_size);

int sensor_repository_get_recent_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size);
int sensor_repository_get_list_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size);
#endif
