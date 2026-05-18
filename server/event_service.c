#include "event_service.h"
#include "event_repository.h"
#include "event_log.h"
#include <stdio.h>
#include <string.h>

#define EVENT_DUP_SECONDS 5

extern EventLog g_event_log;

int event_service_try_add(MYSQL* conn, int plant_id, const char* event_type, const char* message)
{
    if (!conn || !event_type || !message) return -1;

    if (event_repo_exists_recent(conn, plant_id, event_type, EVENT_DUP_SECONDS)) {
        return 0;
    }

    if (event_repo_add(conn, plant_id, event_type, message) != 0) {
        return -1;
    }

    return 0;
}

int event_service_get_recent_from_memory(char* out, size_t out_size)
{
    EventLogItem item;

    if (!out || out_size == 0) return -1;

    if (!event_log_get_recent_item(&g_event_log, 0, &item)) {
        snprintf(out, out_size, "OK []");
        return 0;
    }

    snprintf(out, out_size,
        "OK [{\"id\":%d,\"plant_id\":%d,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}]",
        item.id, item.plant_id, item.event_type, item.message, item.created_at);
    return 0;
}

int event_service_get_list_by_plant_from_memory(const char* req, char* out, size_t out_size)
{
    int plant_id, limit;
    EventLogItem items[64];
    int n, i;
    size_t len = 0;

    if (!req || !out || out_size == 0) return -1;

    if (sscanf(req, "GET_EVENT_LIST_BY_PLANT %d %d", &plant_id, &limit) != 2) {
        snprintf(out, out_size, "ERROR usage: GET_EVENT_LIST_BY_PLANT plant_id limit");
        return -1;
    }

    if (limit < 1) limit = 1;
    if (limit > 64) limit = 64;

    n = event_log_get_recent_list_by_plant(&g_event_log, plant_id, items, limit);

    len += snprintf(out + len, out_size - len, "OK [");
    for (i = 0; i < n && len < out_size; ++i) {
        len += snprintf(out + len, out_size - len,
            "%s{\"id\":%d,\"plant_id\":%d,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}",
            (i == 0 ? "" : ","),
            items[i].id, items[i].plant_id, items[i].event_type,
            items[i].message, items[i].created_at);
    }
    snprintf(out + len, out_size - len, "]");
    return 0;
}

int handle_insert_event_with_conn(MYSQL* conn, int client_sock, const char* cmd)
{
    (void)client_sock;

    int plant_id;
    char event_type[64], message[128];

    if (sscanf(cmd, "INSERT_EVENT %d %63s %127s", &plant_id, event_type, message) != 3) {
        return 0;
    }

    return event_service_try_add(conn, plant_id, event_type, message);
}
