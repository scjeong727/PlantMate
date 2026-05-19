#pragma once

void* mqtt_adapter_thread_main(void* arg);
void mqtt_adapter_publish_sensor(int plant_id, const char* payload);
void mqtt_adapter_publish_status(int plant_id, const char* payload);
void mqtt_adapter_publish_device_command(const char* device_type, const char* device_id, const char* action, const char* payload);
void mqtt_adapter_publish_bridge_command(int plant_id, const char* action, const char* detail);
void mqtt_adapter_publish_device_status(const char* device_type, const char* device_id, const char* payload);
