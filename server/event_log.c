#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "event_log.h"

static int g_event_memory_id = 1;

static void pop_oldest(EventLog* log)
{
    EventLogNode* old_node;

    if (!log || !log->head)
        return;

    old_node = log->head;
    log->head = old_node->next;

    if (log->head)
        log->head->prev = NULL;
    else
        log->tail = NULL;

    free(old_node);

    if (log->size > 0)
        log->size--;
}

int event_log_init(EventLog* log, size_t cap)
{
    if (!log)
        return -1;

    memset(log, 0, sizeof(*log));
    log->cap = (cap == 0) ? 128 : cap;

    pthread_mutex_init(&log->mutex, NULL);
    return 0;
}

void event_log_free(EventLog* log)
{
    if (!log)
        return;

    pthread_mutex_lock(&log->mutex);

    while (log->head)
        pop_oldest(log);

    pthread_mutex_unlock(&log->mutex);
    pthread_mutex_destroy(&log->mutex);
}

void event_log_push(EventLog* log, int plant_id, const char* event_type, const char* message)
{
    EventLogNode* new_node;
    time_t now;
    struct tm tm_now;

    if (!log || !event_type || !message)
        return;

    pthread_mutex_lock(&log->mutex);

    if (log->size > log->cap) {
        while (log->size > log->cap && log->head)
            pop_oldest(log);
    }

    new_node = (EventLogNode*)malloc(sizeof(EventLogNode));
    if (!new_node) {
        pthread_mutex_unlock(&log->mutex);
        return;
    }

    memset(new_node, 0, sizeof(*new_node));
    new_node->data.id = g_event_memory_id++;
    new_node->data.plant_id = plant_id;

    snprintf(new_node->data.event_type, sizeof(new_node->data.event_type), "%s", event_type);
    snprintf(new_node->data.message, sizeof(new_node->data.message), "%s", message);

    now = time(NULL);
    localtime_r(&now, &tm_now);
    strftime(new_node->data.created_at, sizeof(new_node->data.created_at),
             "%Y-%m-%d %H:%M:%S", &tm_now);

    new_node->next = NULL;
    new_node->prev = log->tail;

    if (log->tail)
        log->tail->next = new_node;
    else
        log->head = new_node;

    log->tail = new_node;
    log->size++;

    pthread_mutex_unlock(&log->mutex);
}

void event_log_push_item(EventLog* log, const EventLogItem* item)
{
    EventLogNode* new_node;

    if (!log || !item)
        return;

    pthread_mutex_lock(&log->mutex);

    if (log->size > log->cap) {
        while (log->size > log->cap && log->head)
            pop_oldest(log);
    }
    new_node = (EventLogNode*)malloc(sizeof(EventLogNode));
    if (!new_node) {
        pthread_mutex_unlock(&log->mutex);
        return;
    }

    memset(new_node, 0, sizeof(*new_node));
    new_node->data = *item;
    new_node->next = NULL;
    new_node->prev = log->tail;

    if (log->tail)
        log->tail->next = new_node;
    else
        log->head = new_node;

    log->tail = new_node;
    log->size++;

    if (item->id >= g_event_memory_id)
        g_event_memory_id = item->id + 1;

    pthread_mutex_unlock(&log->mutex);
}

int event_log_get_recent_item(EventLog* log, size_t index, EventLogItem* out)
{
    EventLogNode* cur;
    size_t i;

    if (!log || !out)
        return 0;

    pthread_mutex_lock(&log->mutex);

    if (index >= log->size) {
        pthread_mutex_unlock(&log->mutex);
        return 0;
    }

    cur = log->tail;
    for (i = 0; i < index && cur; ++i)
        cur = cur->prev;

    if (!cur) {
        pthread_mutex_unlock(&log->mutex);
        return 0;
    }

    *out = cur->data;

    pthread_mutex_unlock(&log->mutex);
    return 1;
}

int event_log_get_recent(EventLog* log, size_t index, int* plant_id, char* event_type, char* message)
{
    EventLogItem item;

    if (!event_log_get_recent_item(log, index, &item))
        return 0;

    if (plant_id)
        *plant_id = item.plant_id;
    if (event_type)
        strcpy(event_type, item.event_type);
    if (message)
        strcpy(message, item.message);

    return 1;
}

int event_log_get_recent_list(EventLog* log, EventLogItem* out, int max_count)
{
    EventLogNode* cur;
    int count = 0;

    if (!log || !out || max_count <= 0)
        return 0;

    pthread_mutex_lock(&log->mutex);

    cur = log->tail;
    while (cur && count < max_count) {
        out[count++] = cur->data;
        cur = cur->prev;
    }

    pthread_mutex_unlock(&log->mutex);
    return count;
}

int event_log_get_recent_item_by_plant(EventLog* log, int plant_id, size_t index, EventLogItem* out)
{
    EventLogNode* cur;
    size_t found = 0;

    if (!log || !out || plant_id <= 0)
        return 0;

    pthread_mutex_lock(&log->mutex);

    cur = log->tail;
    while (cur) {
        if (cur->data.plant_id == plant_id) {
            if (found == index) {
                *out = cur->data;
                pthread_mutex_unlock(&log->mutex);
                return 1;
            }
            found++;
        }
        cur = cur->prev;
    }

    pthread_mutex_unlock(&log->mutex);
    return 0;
}

int event_log_get_recent_list_by_plant(EventLog* log, int plant_id, EventLogItem* out, int max_count)
{
    EventLogNode* cur;
    int count = 0;

    if (!log || !out || max_count <= 0 || plant_id <= 0)
        return 0;

    pthread_mutex_lock(&log->mutex);

    cur = log->tail;
    while (cur && count < max_count) {
        if (cur->data.plant_id == plant_id)
            out[count++] = cur->data;
        cur = cur->prev;
    }

    pthread_mutex_unlock(&log->mutex);
    return count;
}

int event_log_get_recent_json(EventLog* log, char* out, size_t out_size)
{
    EventLogItem item;

    if (!log || !out || out_size < 64)
        return 0;

    if (!event_log_get_recent_item(log, 0, &item)) {
        snprintf(out, out_size, "OK {}");
        return 1;
    }

    snprintf(out, out_size,
        "OK {\"id\":%d,\"plant_id\":%d,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}",
        item.id, item.plant_id, item.event_type, item.message, item.created_at);

    return 1;
}

int event_log_get_list_by_plant_json(EventLog* log, int plant_id, int limit, char* out, size_t out_size)
{
    EventLogItem items[64];
    int count;
    int i;
    size_t used = 0;

    if (!log || !out || out_size < 64)
    return 0;

    if (limit < 1) limit = 1;
    if (limit > 64) limit = 64;

    count = event_log_get_recent_list_by_plant(log, plant_id, items, limit);

    used += snprintf(out + used, out_size - used, "OK [");
    for (i = 0; i < count && used < out_size; ++i) {
        used += snprintf(out + used, out_size - used,
            "%s{\"id\":%d,\"plant_id\":%d,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}",
            (i == 0 ? "" : ","),
            items[i].id, items[i].plant_id, items[i].event_type,
            items[i].message, items[i].created_at);
    }
    if (used >= out_size)
      used = out_size - 1;
    snprintf(out + used, out_size - used, "]");

    return 1;
}

int event_log_get_recent_by_plant_json(EventLog* log, int plant_id, char* out, size_t out_size)
{
    EventLogItem item;

    if (!log || !out || out_size < 64)
    return 0;

    if (!event_log_get_recent_item_by_plant(log, plant_id, 0, &item)) {
        snprintf(out, out_size, "OK {}");
        return 1;
    }

    snprintf(out, out_size,
        "OK {\"id\":%d,\"plant_id\":%d,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}",
        item.id, item.plant_id, item.event_type, item.message, item.created_at);

    return 1;
}
