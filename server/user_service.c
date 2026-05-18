#include "user_service.h"
#include "user_repository.h"
#include "session_manager.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
int user_service_add(MYSQL* conn, const char* login_id, const char* password, const char* name)
{
    if (!conn || !login_id || !password || !name) return 0;
    if (strlen(login_id) == 0 || strlen(password) == 0 || strlen(name) == 0) return 0;

    return user_repository_add(conn, login_id, password, name);
}

int user_service_get_all(MYSQL* conn, char* out, size_t out_size)
{
    if (!conn || !out || out_size == 0) return 0;
    return user_repository_get_all(conn, out, out_size);
}

int user_service_remove(MYSQL* conn, int user_id)
{
    if (!conn || user_id <= 0) return 0;
    return user_repository_remove(conn, user_id);
}

int user_service_login(MYSQL* conn, const char* login_id, const char* password, int* out_user_id)
{
    if (!conn || !login_id || !password || !out_user_id) return 0;
    if (strlen(login_id) == 0 || strlen(password) == 0) return 0;

    return user_repository_login(conn, login_id, password, out_user_id);
}

int user_service_find_user_id_by_login_id(MYSQL* conn, const char* login_id, int* out_user_id)
{
    if (!conn || !login_id || !out_user_id) return 0;
    if (strlen(login_id) == 0) return 0;

    return user_repository_find_id_by_login_id(conn, login_id, out_user_id);
}

void handle_add_user_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    char login_id[64], password[64], name[64];

    if (sscanf(buf, "ADD_USER %63s %63s %63s", login_id, password, name) != 3) {
        send(client_sock, "ERROR usage: ADD_USER login_id password name\n", 45, 0);
        return;
    }

    if (user_service_add(conn, login_id, password, name))
        send(client_sock, "OK {\"message\":\"user_added\"}\n", 30, 0);
    else
        send(client_sock, "ERROR add_user_failed\n", 22, 0);
}

void handle_get_user_with_conn(MYSQL* conn, int client_sock)
{
    char out[4096];

    if (user_service_get_all(conn, out, sizeof(out)))
        send(client_sock, out, strlen(out), 0);
    else
        send(client_sock, "ERROR get_user_failed\n", 22, 0);
}

void handle_remove_user_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    int user_id;

    if (sscanf(buf, "REMOVE_USER %d", &user_id) != 1) {
        send(client_sock, "ERROR usage: REMOVE_USER user_id\n", 34, 0);
        return;
    }

    if (user_service_remove(conn, user_id))
        send(client_sock, "OK {\"message\":\"user_removed\"}\n", 32, 0);
    else
        send(client_sock, "ERROR remove_user_failed\n", 25, 0);
}

void handle_login_with_conn(MYSQL* conn, int client_sock, const char* buf)
{
    char login_id[64], password[64];
    int user_id;
    char out[128];

    if (sscanf(buf, "LOGIN %63s %63s", login_id, password) != 2) {
        send(client_sock, "ERROR usage: LOGIN login_id password\n", 38, 0);
        return;
    }

    if (!user_service_login(conn, login_id, password, &user_id)) {
        send(client_sock, "ERROR login_failed\n", 19, 0);
        return;
    }

    if (!session_try_bind_login(client_sock, login_id, user_id)) {
        send(client_sock, "ERROR already_logged_in\n", 25, 0);
        return;
    }

    snprintf(out, sizeof(out), "OK {\"user_id\":%d}\n", user_id);
    send(client_sock, out, strlen(out), 0);
}
