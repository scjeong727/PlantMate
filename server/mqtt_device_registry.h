#pragma once

#include <stddef.h>
#include <time.h>

#define MQTT_DEVICE_ROLE_MAX 32
#define MQTT_DEVICE_TYPE_MAX 32
#define MQTT_DEVICE_ID_MAX 64
#define MQTT_DEVICE_REGISTRY_MAX 128
#define MQTT_DEVICE_STATUS_MAX 256

typedef struct {
    int plant_id;
    char role[MQTT_DEVICE_ROLE_MAX];
    char device_type[MQTT_DEVICE_TYPE_MAX];
    char device_id[MQTT_DEVICE_ID_MAX];
} MqttDeviceBinding;

typedef struct {
    char device_type[MQTT_DEVICE_TYPE_MAX];
    char device_id[MQTT_DEVICE_ID_MAX];
    int online;
    time_t updated_at;
    char status_payload[MQTT_DEVICE_STATUS_MAX];
} MqttLiveDevice;

void mqtt_device_registry_init(void);
int mqtt_device_registry_preload_all(void);
int mqtt_device_registry_bind(int plant_id, const char* role, const char* device_type, const char* device_id);
int mqtt_device_registry_unbind(int plant_id, const char* role);
int mqtt_device_registry_get(int plant_id, const char* role, MqttDeviceBinding* out);
int mqtt_device_registry_find_binding_by_device(const char* role, const char* device_type, const char* device_id, MqttDeviceBinding* out);
int mqtt_device_registry_update_live_device(const char* device_type, const char* device_id, const char* status_payload);
int mqtt_device_registry_list_live_devices_json(const char* device_type, char* out, size_t out_size);
