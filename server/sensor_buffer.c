#include "sensor_buffer.h"
#include <string.h>
#include <stdio.h>

static int g_sensor_memory_id = 1;

void sensor_buffer_init(SensorBuffer* b)
{
    if (!b) return;
    memset(b, 0, sizeof(*b));
    pthread_mutex_init(&b->mutex, NULL);
}

void sensor_buffer_free(SensorBuffer* b)
{
    if (!b) return;
    pthread_mutex_destroy(&b->mutex);
}

void sensor_buffer_push(SensorBuffer* b, const Reading* r)
{
    if (!b || !r) return;

    pthread_mutex_lock(&b->mutex);

    b->items[b->head] = *r;
    if (b->items[b->head].id <= 0) {
        b->items[b->head].id = g_sensor_memory_id++;
    } else if (b->items[b->head].id >= g_sensor_memory_id) {
        g_sensor_memory_id = b->items[b->head].id + 1;
    }

    b->head = (b->head + 1) % SENSOR_BUFFER_CAP;
    if (b->count < SENSOR_BUFFER_CAP) b->count++;

    pthread_mutex_unlock(&b->mutex);
}

void sensor_buffer_push_loaded(SensorBuffer* b, const Reading* r)
{
    if (!b || !r) return;

    pthread_mutex_lock(&b->mutex);

    b->items[b->head] = *r;
    if (r->id >= g_sensor_memory_id) g_sensor_memory_id = r->id + 1;

    b->head = (b->head + 1) % SENSOR_BUFFER_CAP;
    if (b->count < SENSOR_BUFFER_CAP) b->count++;

    pthread_mutex_unlock(&b->mutex);
}

int sensor_buffer_get_recent(SensorBuffer* b, size_t idx, Reading* out)
{
    size_t pos;

    if (!b || !out) return 0;

    pthread_mutex_lock(&b->mutex);

    if (idx >= b->count) {
        pthread_mutex_unlock(&b->mutex);
        return 0;
    }

    pos = (b->head + SENSOR_BUFFER_CAP - 1 - idx) % SENSOR_BUFFER_CAP;
    *out = b->items[pos];

    pthread_mutex_unlock(&b->mutex);
    return 1;
}

int sensor_buffer_get_recent_list_by_plant(SensorBuffer* b, int plant_id, Reading* out, int limit)

{
    int found = 0;
    size_t i, pos;

    if (!b || !out || limit <= 0) return 0;

    pthread_mutex_lock(&b->mutex);

    for (i = 0; i < b->count && found < limit; ++i) {
        pos = (b->head + SENSOR_BUFFER_CAP - 1 - i) % SENSOR_BUFFER_CAP;
        if (b->items[pos].plant_id == plant_id) {
            out[found++] = b->items[pos];
        }
    }

    pthread_mutex_unlock(&b->mutex);
    return found;
}

int sensor_buffer_get_recent_json(SensorBuffer* b, char* out, size_t out_size)
{
    Reading r;

    if (!out || out_size == 0) return 0;

    if (!sensor_buffer_get_recent(b, 0, &r)) {
        snprintf(out, out_size, "OK []");
        return 1;
    }

    snprintf(out, out_size,
        "OK [{\"id\":%d,\"plant_id\":%d,\"temp\":%.2f,\"humi\":%.2f,\"soil\":%d,\"light\":%d,\"created_at\":\"%s\"}]",
        r.id, r.plant_id, r.temp_c, r.humi_pct, r.soil_raw, r.cds_raw, r.created_at);
    return 1;
}

int sensor_buffer_get_list_by_plant_json(SensorBuffer* b, int plant_id, int limit, char* out, size_t out_size)

{
    Reading items[64];
    int n, i;
    size_t len = 0;

    if (!out || out_size == 0) return 0;

    if (limit < 1) limit = 1;
    if (limit > 64) limit = 64;

    n = sensor_buffer_get_recent_list_by_plant(b, plant_id, items, limit);

    len += snprintf(out + len, out_size - len, "OK [");
    for (i = 0; i < n && len < out_size; ++i) {
        len += snprintf(out + len, out_size - len,
            "%s{\"id\":%d,\"plant_id\":%d,\"temp\":%.2f,\"humi\":%.2f,\"soil\":%d,\"light\":%d,\"created_at\":\"%s\"}",
            (i == 0 ? "" : ","),
            items[i].id, items[i].plant_id, items[i].temp_c, items[i].humi_pct,
            items[i].soil_raw, items[i].cds_raw, items[i].created_at);
    }
    snprintf(out + len, out_size - len, "]");
    return 1;
}

int sensor_buffer_get_recent_by_plant_json(SensorBuffer* b, int plant_id, char* out, size_t out_size)
{
    Reading r;
    if (!out || out_size == 0) return 0;
    if (!sensor_buffer_get_recent_list_by_plant(b, plant_id, &r, 1)) {
        snprintf(out, out_size, "OK {}");
        return 1;
    }
    snprintf(out, out_size,
        "OK {\"id\":%d,\"plant_id\":%d,\"temp\":%.2f,\"humi\":%.2f,\"soil\":%d,\"light\":%d,\"created_at\":\"%s\"}",
        r.id, r.plant_id, r.temp_c, r.humi_pct, r.soil_raw, r.cds_raw, r.created_at);
    return 1;
}
