#ifndef EVENT_SERVICE_H
#define EVENT_SERVICE_H

#include <mysql/mysql.h>
#include <stddef.h>

int event_service_try_add(MYSQL* conn, int plant_id, const char* event_type, const char* message);
int event_service_get_recent_from_memory(char* out, size_t out_size);
int event_service_get_list_by_plant_from_memory(const char* req, char* out, size_t out_size);
int handle_insert_event_with_conn(MYSQL* conn, int client_sock, const char* cmd);

#endif
