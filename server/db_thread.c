#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>
#include <sys/socket.h>

#include "db.h"
#include "db_thread.h"
#include "db_queue.h"
#include "user_service.h"
#include "plant_service.h"
#include "sensor_service.h"
#include "event_service.h"
#include "session_manager.h"

extern DBQueue g_db_queue;

static void send_response_safe(int sock, const char* msg)
{
    if (sock < 0 || !msg) return;
    if (send(sock, msg, strlen(msg), 0) <= 0)
        close(sock);
}

static void process_db_request(MYSQL* conn, const DBRequest* req)
{
    const char* buf = req->query;
    int client_sock = req->client_sock;

    if (!conn || !req)
        return;

    if (req->type == DB_REQ_SHUTDOWN)
        return;

    if (!buf)
        return;

    if (strncmp(buf, "ADD_USER ", 9) == 0) {
        handle_add_user_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "GET_USER", 8) == 0) {
        handle_get_user_with_conn(conn, client_sock);
    }
    else if (strncmp(buf, "REMOVE_USER ", 12) == 0) {
        handle_remove_user_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "LOGIN ", 6) == 0) {
        handle_login_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "ADD_PLANT ", 10) == 0) {
        handle_add_plant_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "GET_PLANT_BY_USER ", 18) == 0) {
        handle_get_plant_by_user_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "GET_PLANT", 9) == 0) {
        handle_get_plant_with_conn(conn, client_sock);
    }
    else if (strncmp(buf, "EDIT_PLANT ", 11) == 0) {
        handle_edit_plant_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "REMOVE_PLANT ", 13) == 0) {
        handle_remove_plant_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "WATER_PLANT ", 12) == 0) {
        handle_water_plant_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "POST_SENSOR_DATA ", 17) == 0) {
        handle_post_sensor_data_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "INSERT_EVENT ", 13) == 0) {
        handle_insert_event_with_conn(conn, client_sock, buf);
    }
    else if (strncmp(buf, "GET_RECENT_SENSOR", 17) == 0 ||
             strncmp(buf, "GET_SENSOR_LIST", 15) == 0 ||
             strncmp(buf, "GET_RECENT_EVENT", 16) == 0 ||
             strncmp(buf, "GET_EVENT_LIST", 14) == 0 ||
             strncmp(buf, "GET_SENSOR_LIST_BY_PLANT", 24) == 0 ||
             strncmp(buf, "GET_EVENT_LIST_BY_PLANT", 23) == 0) {
        send_response_safe(client_sock, "ERROR handled_by_request_cache\n");
    }
    else {
        send_response_safe(client_sock, "ERROR unknown_command\n");
    }
}

void* db_thread_main(void* arg)
{
    MYSQL conn;
    DBRequest req;

    (void)arg;

    mysql_init(&conn);
    if (!db_connect(&conn)) {
        fprintf(stderr, "DB thread: db_connect failed\n");
        return NULL;
    }

    while (1) {
        db_queue_pop(&g_db_queue, &req);

        if (req.type == DB_REQ_SHUTDOWN)
            break;

        process_db_request(&conn, &req);
    }

    mysql_close(&conn);
    return NULL;
}
