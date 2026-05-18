#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

#include <mysql/mysql.h>
#include <stddef.h>

int user_repository_add(MYSQL* conn, const char* login_id, const char* password, const char* name);
int user_repository_get_all(MYSQL* conn, char* out, size_t out_size);
int user_repository_remove(MYSQL* conn, int user_id);
int user_repository_login(MYSQL* conn, const char* login_id, const char* password, int* out_user_id);
int user_repository_find_id_by_login_id(MYSQL* conn, const char* login_id, int* out_user_id);

#endif
