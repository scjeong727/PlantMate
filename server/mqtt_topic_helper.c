#include "mqtt_topic_helper.h"

#include <stdio.h>
#include <string.h>

static void mqtt_topic_clear(char* out, size_t out_size)
{
    if (out && out_size > 0)
        out[0] = '\0';
}

void mqtt_topic_build_plant_sensor(int plant_id, char* out, size_t out_size)
{
    if (plant_id <= 0 || !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "plant/%d/sensor", plant_id);
}

void mqtt_topic_build_plant_status(int plant_id, char* out, size_t out_size)
{
    if (plant_id <= 0 || !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "plant/%d/status", plant_id);
}

void mqtt_topic_build_plant_water_command(int plant_id, char* out, size_t out_size)
{
    if (plant_id <= 0 || !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "plant/%d/water/command", plant_id);
}

void mqtt_topic_build_app_request(const char* client_id, char* out, size_t out_size)
{
    if (!client_id || client_id[0] == '\0' || !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "app/%s/request", client_id);
}

void mqtt_topic_build_app_response(const char* client_id, char* out, size_t out_size)
{
    if (!client_id || client_id[0] == '\0' || !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "app/%s/response", client_id);
}

void mqtt_topic_build_device_command(const char* device_type, const char* device_id, const char* action, char* out, size_t out_size)
{
    if (!device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0' ||
        !action || action[0] == '\0' ||
        !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "device/%s/%s/%s/command", device_type, device_id, action);
}

void mqtt_topic_build_device_status(const char* device_type, const char* device_id, char* out, size_t out_size)
{
    if (!device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0' ||
        !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "device/%s/%s/status", device_type, device_id);
}

void mqtt_topic_build_device_telemetry(const char* device_type, const char* device_id, char* out, size_t out_size)
{
    if (!device_type || device_type[0] == '\0' ||
        !device_id || device_id[0] == '\0' ||
        !out || out_size == 0) {
        mqtt_topic_clear(out, out_size);
        return;
    }
    snprintf(out, out_size, "device/%s/%s/telemetry", device_type, device_id);
}
