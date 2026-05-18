#include <pthread.h>
#include <string.h>
#include "plant_owner_cache.h"

#define MAX_PLANT_OWNER_CACHE 512

typedef struct {
    int plant_id;
    int user_id;
} PlantOwnerItem;

static PlantOwnerItem g_items[MAX_PLANT_OWNER_CACHE];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void plant_owner_cache_init(void)
{
    pthread_mutex_lock(&g_mutex);
    memset(g_items, 0, sizeof(g_items));
    pthread_mutex_unlock(&g_mutex);
}

void plant_owner_cache_set(int plant_id, int user_id)
{
    int i;
    int empty_idx = -1;

    if (plant_id <= 0 || user_id <= 0)
        return;

    pthread_mutex_lock(&g_mutex);

    for (i = 0; i < MAX_PLANT_OWNER_CACHE; ++i) {
        if (g_items[i].plant_id == plant_id) {
            g_items[i].user_id = user_id;
            pthread_mutex_unlock(&g_mutex);
            return;
        }
        if (g_items[i].plant_id == 0 && empty_idx == -1)
            empty_idx = i;
    }

    if (empty_idx != -1) {
        g_items[empty_idx].plant_id = plant_id;
        g_items[empty_idx].user_id = user_id;
    }

    pthread_mutex_unlock(&g_mutex);
}

void plant_owner_cache_remove(int plant_id)
{
    int i;

    if (plant_id <= 0)
        return;

    pthread_mutex_lock(&g_mutex);

    for (i = 0; i < MAX_PLANT_OWNER_CACHE; ++i) {
        if (g_items[i].plant_id == plant_id) {
            g_items[i].plant_id = 0;
            g_items[i].user_id = 0;
            break;
        }
    }

    pthread_mutex_unlock(&g_mutex);
}

int plant_owner_cache_exists_by_user(int plant_id, int user_id)
{
    int i;
    int found = 0;

    if (plant_id <= 0 || user_id <= 0)
        return 0;

    pthread_mutex_lock(&g_mutex);

    for (i = 0; i < MAX_PLANT_OWNER_CACHE; ++i) {
        if (g_items[i].plant_id == plant_id && g_items[i].user_id == user_id) {
            found = 1;
            break;
        }
    }

    pthread_mutex_unlock(&g_mutex);
    return found;
}
