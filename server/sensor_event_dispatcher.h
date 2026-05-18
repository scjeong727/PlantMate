#pragma once

typedef void (*SensorEventNotifyFn)(int plant_id, const char* message);

void sensor_event_dispatcher_evaluate(
    int client_sock,
    int plant_id,
    double temp,
    double humi,
    int soil,
    int light,
    SensorEventNotifyFn notify_fn);
