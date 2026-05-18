#include <pthread.h>
#include <string.h>
#include "watering_lock.h"

#define MAX_WATERING_PLANTS 256

static pthread_mutex_t g_watering_lock_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_watering_plants[MAX_WATERING_PLANTS];

void watering_lock_init(void)
{
    pthread_mutex_lock(&g_watering_lock_mutex);
    memset(g_watering_plants, 0, sizeof(g_watering_plants));
    pthread_mutex_unlock(&g_watering_lock_mutex);
}

int watering_try_begin(int plant_id)
{
    int i;
    int empty_idx = -1;

    if (plant_id <= 0)
        return 0;

    pthread_mutex_lock(&g_watering_lock_mutex);

    for (i = 0; i < MAX_WATERING_PLANTS; ++i) {
        if (g_watering_plants[i] == plant_id) {
            pthread_mutex_unlock(&g_watering_lock_mutex);
            return 0;
        }
        if (g_watering_plants[i] == 0 && empty_idx == -1)
            empty_idx = i;
    }

    if (empty_idx == -1) {
        pthread_mutex_unlock(&g_watering_lock_mutex);
        return 0;
    }

    g_watering_plants[empty_idx] = plant_id;
    pthread_mutex_unlock(&g_watering_lock_mutex);
    return 1;
}

void watering_end(int plant_id)
{
    int i;

    if (plant_id <= 0)
        return;

    pthread_mutex_lock(&g_watering_lock_mutex);

    for (i = 0; i < MAX_WATERING_PLANTS; ++i) {
        if (g_watering_plants[i] == plant_id) {
            g_watering_plants[i] = 0;
            break;
        }
    }

    pthread_mutex_unlock(&g_watering_lock_mutex);
}
