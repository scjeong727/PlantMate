#include "plant_threshold_cache.h"
#include <string.h>

PlantThresholdCache g_plant_threshold_cache;

void plant_threshold_cache_init(void)
{
    memset(&g_plant_threshold_cache, 0, sizeof(g_plant_threshold_cache));
    pthread_mutex_init(&g_plant_threshold_cache.mutex, NULL);
}

void plant_threshold_cache_destroy(void)
{
    pthread_mutex_destroy(&g_plant_threshold_cache.mutex);
}

void plant_threshold_cache_clear(void)
{
    pthread_mutex_lock(&g_plant_threshold_cache.mutex);
    g_plant_threshold_cache.count = 0;
    pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
}

int plant_threshold_cache_set(
    int plant_id,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max
)
{
    int i;
    PlantThreshold* item = NULL;

    pthread_mutex_lock(&g_plant_threshold_cache.mutex);

    for (i = 0; i < g_plant_threshold_cache.count; ++i) {
        if (g_plant_threshold_cache.items[i].plant_id == plant_id) {
            item = &g_plant_threshold_cache.items[i];
            break;
        }
    }

    if (!item) {
        if (g_plant_threshold_cache.count >= PLANT_THRESHOLD_CACHE_MAX) {
            pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
            return 0;
        }
        item = &g_plant_threshold_cache.items[g_plant_threshold_cache.count++];
    }

    item->plant_id = plant_id;
    item->temp_min = temp_min;
    item->temp_max = temp_max;
    item->humi_min = humi_min;
    item->humi_max = humi_max;
    item->soil_min = soil_min;
    item->soil_max = soil_max;
    item->light_min = light_min;
    item->light_max = light_max;

    pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
    return 1;
}

int plant_threshold_cache_get(int plant_id, PlantThreshold* out)
{
    int i;

    if (!out) return 0;

    pthread_mutex_lock(&g_plant_threshold_cache.mutex);

    for (i = 0; i < g_plant_threshold_cache.count; ++i) {
        if (g_plant_threshold_cache.items[i].plant_id == plant_id) {
            *out = g_plant_threshold_cache.items[i];
            pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
    return 0;
}

int plant_threshold_cache_remove(int plant_id)
{
    int i;

    pthread_mutex_lock(&g_plant_threshold_cache.mutex);

    for (i = 0; i < g_plant_threshold_cache.count; ++i) {
        if (g_plant_threshold_cache.items[i].plant_id == plant_id) {
            g_plant_threshold_cache.items[i] =
                g_plant_threshold_cache.items[g_plant_threshold_cache.count - 1];
            g_plant_threshold_cache.count--;
            pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_plant_threshold_cache.mutex);
    return 0;
}
