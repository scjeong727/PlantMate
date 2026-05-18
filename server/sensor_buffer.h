#ifndef SENSOR_BUFFER_H
#define SENSOR_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#define SENSOR_BUFFER_CAP 256

typedef struct {
    int id;
    int plant_id;
    uint64_t ts_ms;
    double temp_c;
    double humi_pct;
    int soil_raw;
    int cds_raw;
    char created_at[32];
} Reading;

typedef struct {
    Reading items[SENSOR_BUFFER_CAP];
    size_t head;
    size_t count;
    pthread_mutex_t mutex;
} SensorBuffer;

void sensor_buffer_init(SensorBuffer* b);
void sensor_buffer_free(SensorBuffer* b);
void sensor_buffer_push(SensorBuffer* b, const Reading* r);
void sensor_buffer_push_loaded(SensorBuffer* b, const Reading* r);

int sensor_buffer_get_recent(SensorBuffer* b, size_t idx, Reading* out);
int sensor_buffer_get_recent_list_by_plant(SensorBuffer* b, int plant_id, Reading* out, int limit);

int sensor_buffer_get_recent_json(SensorBuffer* b, char* out, size_t out_size);
int sensor_buffer_get_list_by_plant_json(SensorBuffer* b, int plant_id, int limit, char* out, size_t out_size);
int sensor_buffer_get_recent_by_plant_json(SensorBuffer* b, int plant_id, char* out, size_t out_size);
#endif
