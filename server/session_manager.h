#pragma once
#include <stdbool.h>

bool session_try_bind_login(int client_sock, const char* login_id, int user_id);
void session_remove_by_sock(int client_sock);
bool session_is_login_active(const char* login_id);
int session_get_user_id_by_sock(int client_sock);
