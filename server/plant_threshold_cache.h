#ifndef PLANT_THRESHOLD_CACHE_H
#define PLANT_THRESHOLD_CACHE_H

#include <pthread.h>

#define PLANT_THRESHOLD_CACHE_MAX 256

typedef struct {
    int plant_id;
    double temp_min;
    double temp_max;
    double humi_min;
    double humi_max;
    int soil_min;
    int soil_max;
    int light_min;
    int light_max;
} PlantThreshold;

typedef struct {
    PlantThreshold items[PLANT_THRESHOLD_CACHE_MAX];
    int count;
    pthread_mutex_t mutex;
} PlantThresholdCache;

extern PlantThresholdCache g_plant_threshold_cache;

void plant_threshold_cache_init(void);
void plant_threshold_cache_destroy(void);
void plant_threshold_cache_clear(void);

int plant_threshold_cache_set(
    int plant_id,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max
);

int plant_threshold_cache_get(int plant_id, PlantThreshold* out);
int plant_threshold_cache_remove(int plant_id);

#endif
