#ifndef EVENT_LOG_H
#define EVENT_LOG_H
#include <stddef.h>
#include <pthread.h>

typedef struct {
    int id;
    int plant_id;
    char event_type[64];
    char message[128];
    char created_at[20];
} EventLogItem;

typedef struct EventLogNode{
    EventLogItem data;
    struct EventLogNode* next;
    struct EventLogNode* prev;
} EventLogNode;

typedef struct{
    EventLogNode* head;
    EventLogNode* tail;
    size_t cap;
    size_t size;
    pthread_mutex_t mutex;
} EventLog;

int event_log_init(EventLog* log, size_t cap);
void event_log_free(EventLog* log);
void event_log_push(EventLog* log, int plant_id, const char* event_type, const char* message);
int event_log_get_recent(EventLog* log, size_t index, int* plant_id, char* event_type, char* message);
int event_log_get_recent_item(EventLog* log, size_t index, EventLogItem* out);
int event_log_get_recent_list(EventLog* log, EventLogItem* out, int max_count);
int event_log_get_recent_item_by_plant(EventLog* log, int plant_id, size_t index, EventLogItem* out);
int event_log_get_recent_list_by_plant(EventLog* log, int plant_id, EventLogItem* out, int max_count);
void event_log_push_item(EventLog* log, const EventLogItem* item);
int event_log_get_recent_json(EventLog* log, char* out, size_t out_size);
int event_log_get_list_by_plant_json(EventLog* log, int plant_id, int limit, char* out, size_t out_size);
int event_log_get_recent_by_plant_json(EventLog* log, int plant_id, char* out, size_t out_size);
#endif
