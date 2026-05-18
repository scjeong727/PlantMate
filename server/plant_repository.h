#ifndef PLANT_REPOSITORY_H
#define PLANT_REPOSITORY_H

#include <mysql/mysql.h>
#include <stddef.h>

int plant_repository_add(
    MYSQL* conn,
    int user_id,
    const char* name,
    const char* type,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max
);

int plant_repository_get_all(MYSQL* conn, char* out, size_t out_size);
int plant_repository_get_by_user(MYSQL* conn, int user_id, char* out, size_t out_size);
int plant_repository_remove(MYSQL* conn, int plant_id, int user_id);

int plant_repository_edit(
    MYSQL* conn,
    int plant_id,
    int user_id,
    const char* name,
    const char* type,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max
);

int plant_repository_remove_sensor_data_by_plant(MYSQL* conn, int plant_id);
int plant_repository_remove_events_by_plant(MYSQL* conn, int plant_id);
int plant_repository_exists_by_user(MYSQL* conn, int plant_id, int user_id);

#endif
