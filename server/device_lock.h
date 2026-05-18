#ifndef DEVICE_LOCK_H
#define DEVICE_LOCK_H

#include <pthread.h>
#include <stddef.h>

#define DEVICE_PATH_MAX 256
#define MAX_DEVICE_LOCKS 16

typedef struct
{
    int owner_sock;
    char device_path[DEVICE_PATH_MAX];
    int sensor_enabled;
    int plant_id;
} DeviceLockItem;

typedef struct
{
    DeviceLockItem items[MAX_DEVICE_LOCKS];
    pthread_mutex_t mutex;
} DeviceLock;

typedef struct
{
    int index;
    int plant_id;
    char device_path[DEVICE_PATH_MAX];
} ActiveSensorInfo;

void device_lock_init(DeviceLock* lock);
void device_lock_cleanup(DeviceLock* lock);

int device_lock_is_available(DeviceLock* lock, const char* device_path);
int device_lock_acquire(DeviceLock* lock, const char* device_path, int owner_sock);
int device_lock_release_by_owner(DeviceLock* lock, int owner_sock);

int device_lock_enable_sensor_stream(DeviceLock* lock, int owner_sock, int plant_id);
int device_lock_disable_sensor_stream(DeviceLock* lock, int owner_sock);

int device_lock_get_device_by_owner(
    DeviceLock* lock,
    int owner_sock,
    char* out,
    size_t out_size
);

int device_lock_get_active_sensor_list(
    DeviceLock* lock,
    ActiveSensorInfo* out,
    int max_count
);

#endif