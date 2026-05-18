#pragma once

#include <stddef.h>

void mqtt_topic_build_plant_sensor(int plant_id, char* out, size_t out_size);
void mqtt_topic_build_plant_status(int plant_id, char* out, size_t out_size);
void mqtt_topic_build_plant_water_command(int plant_id, char* out, size_t out_size);
void mqtt_topic_build_app_request(const char* client_id, char* out, size_t out_size);
void mqtt_topic_build_app_response(const char* client_id, char* out, size_t out_size);
void mqtt_topic_build_device_command(const char* device_type, const char* device_id, const char* action, char* out, size_t out_size);
void mqtt_topic_build_device_status(const char* device_type, const char* device_id, char* out, size_t out_size);
void mqtt_topic_build_device_telemetry(const char* device_type, const char* device_id, char* out, size_t out_size);
