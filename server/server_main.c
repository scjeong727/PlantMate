#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include "db_queue.h"
#include "db_thread.h"
#include "sensor_thread.h"
#include "request_thread.h"
#include "command_queue.h"
#include "watering_thread.h"
#include "sensor_buffer.h"
#include "event_log.h"
#include "cache_preload.h"
#include "sensing.h"
#include "watering_lock.h"
#include "plant_owner_cache.h"
#include "cache_preload.h"
#include "plant_threshold_cache.h"
#include "device_lock.h"
#include "mqtt_adapter.h"
#include "mqtt_device_registry.h"
#include "server_config.h"

DBQueue g_db_queue;
CommandQueue g_command_queue;
SensorBuffer g_sensor_buffer;
EventLog g_event_log;
DeviceLock g_sensor_device_lock;
DeviceLock g_water_device_lock;

int main()
{
    
    pthread_t db_thread;
    pthread_t sensor_thread;
    pthread_t request_thread;
    pthread_t watering_thread;
    pthread_t sensing_thread;
    pthread_t mqtt_thread;
    signal(SIGPIPE, SIG_IGN);
    server_config_load();
    
    db_queue_init(&g_db_queue);
    event_log_init(&g_event_log, 128);
    
    device_lock_init(&g_sensor_device_lock);
    device_lock_init(&g_water_device_lock);
    plant_threshold_cache_init();
    mqtt_device_registry_init();
    mqtt_device_registry_preload_all();
    command_queue_init(&g_command_queue);
    sensor_buffer_init(&g_sensor_buffer);
    watering_lock_init();
    plant_owner_cache_init();
    plant_owner_cache_preload_all();


    if (!cache_preload_all()) {
        printf("server start failed: preload failed\n");
        return 1;
    }
    
    pthread_create(&db_thread, NULL, db_thread_main, NULL);
    pthread_create(&sensor_thread, NULL, sensor_thread_main, NULL);
    pthread_create(&request_thread, NULL, request_thread_main, NULL);
    pthread_create(&watering_thread, NULL, watering_thread_main, NULL);
    pthread_create(&sensing_thread, NULL, sensing_thread_main, NULL);
    pthread_create(&mqtt_thread, NULL, mqtt_adapter_thread_main, NULL);
    
    pthread_join(request_thread, NULL);
    pthread_join(sensor_thread, NULL);
    pthread_join(db_thread, NULL);
    pthread_join(watering_thread, NULL);
    pthread_join(sensing_thread, NULL);
    pthread_join(mqtt_thread, NULL);
    
    return 0;
}
