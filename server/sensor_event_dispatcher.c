#include <stdio.h>
#include <string.h>

#include "sensor_event_dispatcher.h"
#include "db_queue.h"
#include "event_log.h"
#include "plant_threshold_cache.h"

extern DBQueue g_db_queue;
extern EventLog g_event_log;

static void push_db_req(int client_sock, const char* text)
{
    DBRequest req;

    memset(&req, 0, sizeof(req));
    req.type = DB_REQ_SENSOR;
    req.client_sock = client_sock;
    snprintf(req.query, DB_QUERY_SIZE, "%s", text);
    db_queue_push(&g_db_queue, &req);
}

static void push_event_req(
    int client_sock,
    int plant_id,
    const char* event_type,
    const char* message,
    SensorEventNotifyFn notify_fn)
{
    char event_req[256];
    char status_payload[256];

    event_log_push(&g_event_log, plant_id, event_type, message);

    snprintf(event_req, sizeof(event_req),
        "INSERT_EVENT %d %s %s",
        plant_id, event_type, message);
    push_db_req(client_sock, event_req);

    if (notify_fn) {
        snprintf(status_payload, sizeof(status_payload),
            "{\"eventType\":\"%s\",\"message\":\"%s\"}",
            event_type, message);
        notify_fn(plant_id, status_payload);
    }
}

void sensor_event_dispatcher_evaluate(
    int client_sock,
    int plant_id,
    double temp,
    double humi,
    int soil,
    int light,
    SensorEventNotifyFn notify_fn)
{
    PlantThreshold threshold;

    if (!plant_threshold_cache_get(plant_id, &threshold))
        return;

    if (temp < threshold.temp_min) {
        push_event_req(client_sock, plant_id, "TEMP_LOW_ALERT", "Temperature_too_low", notify_fn);
    }
    else if (temp > threshold.temp_max) {
        push_event_req(client_sock, plant_id, "TEMP_HIGH_ALERT", "Temperature_too_high", notify_fn);
    }

    if (humi < threshold.humi_min) {
        push_event_req(client_sock, plant_id, "HUMI_LOW_ALERT", "Humidity_too_low", notify_fn);
    }
    else if (humi > threshold.humi_max) {
        push_event_req(client_sock, plant_id, "HUMI_HIGH_ALERT", "Humidity_too_high", notify_fn);
    }

    if (soil < threshold.soil_min) {
        push_event_req(client_sock, plant_id, "SOIL_LOW_ALERT", "Soil_moisture_too_low", notify_fn);
    }
    else if (soil > threshold.soil_max) {
        push_event_req(client_sock, plant_id, "SOIL_HIGH_ALERT", "Soil_moisture_too_high", notify_fn);
    }

    if (light < threshold.light_min) {
        push_event_req(client_sock, plant_id, "LIGHT_LOW_ALERT", "Light_too_low", notify_fn);
    }
    else if (light > threshold.light_max) {
        push_event_req(client_sock, plant_id, "LIGHT_HIGH_ALERT", "Light_too_high", notify_fn);
    }
}
