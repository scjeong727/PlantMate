#ifndef EVENT_REPOSITORY_H
#define EVENT_REPOSITORY_H

#include <mysql/mysql.h>
#include <stddef.h>

int event_repo_add(MYSQL* conn, int plant_id, const char* event_type, const char* message);
int event_repo_exists_recent(MYSQL* conn, int plant_id, const char* event_type, int seconds);
int event_repo_get_recent_json(MYSQL* conn, char* out, size_t out_size);
int event_repo_get_list_json(MYSQL* conn, char* out, size_t out_size);
int event_repository_get_recent_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size);
int event_repository_get_list_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size);
#endif
