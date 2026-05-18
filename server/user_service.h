#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include <mysql/mysql.h>
#include <stddef.h>

int user_service_add(MYSQL* conn, const char* login_id, const char* password, const char* name);
int user_service_get_all(MYSQL* conn, char* out, size_t out_size);
int user_service_remove(MYSQL* conn, int user_id);
int user_service_login(MYSQL* conn, const char* login_id, const char* password, int* out_user_id);
int user_service_find_user_id_by_login_id(MYSQL* conn, const char* login_id, int* out_user_id);

void handle_add_user_with_conn(MYSQL* conn, int client_sock, const char* buf);
void handle_get_user_with_conn(MYSQL* conn, int client_sock);
void handle_remove_user_with_conn(MYSQL* conn, int client_sock, const char* buf);
void handle_login_with_conn(MYSQL* conn, int client_sock, const char* buf);

#endif
