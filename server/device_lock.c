#include "device_lock.h"
#include <string.h>

static int find_by_path_nolock(DeviceLock* lock, const char* device_path)
{
    int i;

    for (i = 0; i < MAX_DEVICE_LOCKS; ++i) {
        if (lock->items[i].owner_sock < 0)
            continue;
        if (strcmp(lock->items[i].device_path, device_path) == 0)
            return i;
    }

    return -1;
}

static int find_by_owner_nolock(DeviceLock* lock, int owner_sock)
{
    int i;

    for (i = 0; i < MAX_DEVICE_LOCKS; ++i) {
        if (lock->items[i].owner_sock == owner_sock)
            return i;
    }

    return -1;
}

static int find_empty_nolock(DeviceLock* lock)
{
    int i;

    for (i = 0; i < MAX_DEVICE_LOCKS; ++i) {
        if (lock->items[i].owner_sock < 0)
            return i;
    }

    return -1;
}

void device_lock_init(DeviceLock* lock)
{
    int i;

    if (!lock)
        return;

    memset(lock, 0, sizeof(DeviceLock));
    pthread_mutex_init(&lock->mutex, NULL);

    for (i = 0; i < MAX_DEVICE_LOCKS; ++i) {
        lock->items[i].owner_sock = -1;
        lock->items[i].device_path[0] = '\0';
        lock->items[i].sensor_enabled = 0;
        lock->items[i].plant_id = -1;
    }
}

void device_lock_cleanup(DeviceLock* lock)
{
    if (!lock)
        return;

    pthread_mutex_destroy(&lock->mutex);
}

int device_lock_is_available(DeviceLock* lock, const char* device_path)
{
    int ok;

    if (!lock || !device_path || device_path[0] == '\0')
        return 0;

    pthread_mutex_lock(&lock->mutex);
    ok = (find_by_path_nolock(lock, device_path) < 0);
    pthread_mutex_unlock(&lock->mutex);

    return ok;
}

int device_lock_acquire(DeviceLock* lock, const char* device_path, int owner_sock)
{
    int by_path;
    int by_owner;
    int empty_idx;

    if (!lock || !device_path || device_path[0] == '\0' || owner_sock < 0)
        return 0;

    pthread_mutex_lock(&lock->mutex);

    by_path = find_by_path_nolock(lock, device_path);
    if (by_path >= 0) {
        if (lock->items[by_path].owner_sock == owner_sock) {
            pthread_mutex_unlock(&lock->mutex);
            return 1;
        }
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    by_owner = find_by_owner_nolock(lock, owner_sock);
    if (by_owner >= 0) {
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    empty_idx = find_empty_nolock(lock);
    if (empty_idx < 0) {
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    lock->items[empty_idx].owner_sock = owner_sock;
    strncpy(lock->items[empty_idx].device_path,
            device_path,
            sizeof(lock->items[empty_idx].device_path) - 1);
    lock->items[empty_idx].device_path[sizeof(lock->items[empty_idx].device_path) - 1] = '\0';
    lock->items[empty_idx].sensor_enabled = 0;
    lock->items[empty_idx].plant_id = -1;

    pthread_mutex_unlock(&lock->mutex);
    return 1;
}

int device_lock_release_by_owner(DeviceLock* lock, int owner_sock)
{
    int idx;

    if (!lock || owner_sock < 0)
        return 0;

    pthread_mutex_lock(&lock->mutex);

    idx = find_by_owner_nolock(lock, owner_sock);
    if (idx < 0) {
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    lock->items[idx].owner_sock = -1;
    lock->items[idx].device_path[0] = '\0';
    lock->items[idx].sensor_enabled = 0;
    lock->items[idx].plant_id = -1;

    pthread_mutex_unlock(&lock->mutex);
    return 1;
}

int device_lock_enable_sensor_stream(DeviceLock* lock, int owner_sock, int plant_id)
{
    int idx;

    if (!lock || owner_sock < 0 || plant_id <= 0)
        return 0;

    pthread_mutex_lock(&lock->mutex);

    idx = find_by_owner_nolock(lock, owner_sock);
    if (idx < 0) {
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    lock->items[idx].sensor_enabled = 1;
    lock->items[idx].plant_id = plant_id;

    pthread_mutex_unlock(&lock->mutex);
    return 1;
}

int device_lock_disable_sensor_stream(DeviceLock* lock, int owner_sock)
{
    int idx;

    if (!lock || owner_sock < 0)
        return 0;

    pthread_mutex_lock(&lock->mutex);

    idx = find_by_owner_nolock(lock, owner_sock);
    if (idx < 0) {
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    lock->items[idx].sensor_enabled = 0;
    lock->items[idx].plant_id = -1;

    pthread_mutex_unlock(&lock->mutex);
    return 1;
}

int device_lock_get_device_by_owner(
    DeviceLock* lock,
    int owner_sock,
    char* out,
    size_t out_size
)
{
    int idx;

    if (!lock || owner_sock < 0 || !out || out_size == 0)
        return 0;

    pthread_mutex_lock(&lock->mutex);

    idx = find_by_owner_nolock(lock, owner_sock);
    if (idx < 0 || lock->items[idx].device_path[0] == '\0') {
        pthread_mutex_unlock(&lock->mutex);
        return 0;
    }

    strncpy(out, lock->items[idx].device_path, out_size - 1);
    out[out_size - 1] = '\0';

    pthread_mutex_unlock(&lock->mutex);
    return 1;
}

int device_lock_get_active_sensor_list(
    DeviceLock* lock,
    ActiveSensorInfo* out,
    int max_count
)
{
    int i;
    int count = 0;

    if (!lock || !out || max_count <= 0)
        return 0;

    pthread_mutex_lock(&lock->mutex);

    for (i = 0; i < MAX_DEVICE_LOCKS && count < max_count; ++i) {
        if (lock->items[i].owner_sock < 0)
            continue;
        if (!lock->items[i].sensor_enabled)
            continue;
        if (lock->items[i].plant_id <= 0)
            continue;
        if (lock->items[i].device_path[0] == '\0')
            continue;

        out[count].index = i;
        out[count].plant_id = lock->items[i].plant_id;
        strncpy(out[count].device_path,
                lock->items[i].device_path,
                sizeof(out[count].device_path) - 1);
        out[count].device_path[sizeof(out[count].device_path) - 1] = '\0';
        ++count;
    }

    pthread_mutex_unlock(&lock->mutex);
    return count;
}