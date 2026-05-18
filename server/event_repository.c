#include "event_repository.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int append_text(char* out, size_t out_size, const char* text)
{
    size_t cur;
    size_t remain;
    int written;

    if (!out || !text || out_size == 0) return 0;

    cur = strlen(out);
    if (cur >= out_size) return 0;

    remain = out_size - cur;
    written = snprintf(out + cur, remain, "%s", text);
    if (written < 0 || (size_t)written >= remain) return 0;

    return 1;
}

int event_repo_add(MYSQL* conn, int plant_id, const char* event_type, const char* message)
{
    char esc_type[128];
    char esc_msg[512];
    char query[1024];

    if (!conn || !event_type || !message) return -1;

    mysql_real_escape_string(conn, esc_type, event_type, (unsigned long)strlen(event_type));
    mysql_real_escape_string(conn, esc_msg, message, (unsigned long)strlen(message));

    snprintf(
        query,
        sizeof(query),
        "INSERT INTO events "
        "(plant_id, event_type, message, created_at) "
        "VALUES (%d, '%s', '%s', NOW())",
        plant_id, esc_type, esc_msg
    );

    if (mysql_query(conn, query) != 0) {
        return -1;
    }

    return 0;
}

int event_repo_exists_recent(MYSQL* conn, int plant_id, const char* event_type, int seconds)
{
    char esc_type[128];
    char query[512];
    MYSQL_RES* res;
    MYSQL_ROW row;
    int exists = 0;

    if (!conn || !event_type) return 0;

    mysql_real_escape_string(conn, esc_type, event_type, (unsigned long)strlen(event_type));

    snprintf(
        query,
        sizeof(query),
        "SELECT COUNT(*) "
        "FROM events "
        "WHERE plant_id=%d "
        "AND event_type='%s' "
        "AND created_at >= DATE_SUB(NOW(), INTERVAL %d SECOND)",
        plant_id, esc_type, seconds
    );

    if (mysql_query(conn, query) != 0) {
        return 0;
    }

    res = mysql_store_result(conn);
    if (!res) {
        return 0;
    }

    row = mysql_fetch_row(res);
    if (row && row[0] && atoi(row[0]) > 0) {
        exists = 1;
    }

    mysql_free_result(res);
    return exists;
}

int event_repo_get_recent_json(MYSQL* conn, char* out, size_t out_size)
{
    const char* query =
        "SELECT id, plant_id, event_type, message, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM events "
        "ORDER BY id DESC LIMIT 1";

    MYSQL_RES* res;
    MYSQL_ROW row;

    if (!conn || !out || out_size == 0) return -1;

    if (mysql_query(conn, query) != 0) {
        return -1;
    }

    res = mysql_store_result(conn);
    if (!res) {
        return -1;
    }

    row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return -1;
    }

    snprintf(
        out,
        out_size,
        "{\"id\":%s,\"plant_id\":%s,\"event_type\":\"%s\","
        "\"message\":\"%s\",\"created_at\":\"%s\"}",
        row[0], row[1], row[2], row[3], row[4]
    );

    mysql_free_result(res);
    return 0;
}

int event_repo_get_list_json(MYSQL* conn, char* out, size_t out_size)
{
    const char* query =
        "SELECT id, plant_id, event_type, message, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM events "
        "ORDER BY id DESC LIMIT 10";

    MYSQL_RES* res;
    MYSQL_ROW row;
    int first = 1;

    if (!conn || !out || out_size == 0) return -1;

    if (mysql_query(conn, query) != 0) {
        return -1;
    }

    res = mysql_store_result(conn);
    if (!res) {
        return -1;
    }

    out[0] = '\0';

    if (!append_text(out, out_size, "[")) {
        mysql_free_result(res);
        return -1;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        char item[512];

        snprintf(
            item,
            sizeof(item),
            "%s{\"id\":%s,\"plant_id\":%s,\"event_type\":\"%s\","
            "\"message\":\"%s\",\"created_at\":\"%s\"}",
            first ? "" : ",",
            row[0], row[1], row[2], row[3], row[4]
        );

        if (!append_text(out, out_size, item)) {
            mysql_free_result(res);
            return -1;
        }

        first = 0;
    }

    if (!append_text(out, out_size, "]")) {
        mysql_free_result(res);
        return -1;
    }

    mysql_free_result(res);
    return 0;
}

int event_repository_get_recent_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size)
{
    char query[256];
    MYSQL_RES* res;
    MYSQL_ROW row;

    if (!conn || !out || out_size == 0) return -1;

    snprintf(query, sizeof(query),
        "SELECT id, plant_id, event_type, message, created_at "
        "FROM events WHERE plant_id=%d ORDER BY created_at DESC LIMIT 1",
        plant_id);

    if (mysql_query(conn, query) != 0) return -1;

    res = mysql_store_result(conn);
    if (!res) return -1;

    row = mysql_fetch_row(res);
    if (!row) {
        snprintf(out, out_size, "{}");
        mysql_free_result(res);
        return 0;
    }

    snprintf(out, out_size,
        "{\"id\":%s,\"plant_id\":%s,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}",
        row[0], row[1], row[2], row[3], row[4]);

    mysql_free_result(res);
    return 0;
}

int event_repository_get_list_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size)
{
    char query[256];
    MYSQL_RES* res;
    MYSQL_ROW row;
    int first = 1;

    if (!conn || !out) return -1;

    snprintf(query, sizeof(query),
        "SELECT id, plant_id, event_type, message, created_at "
        "FROM events WHERE plant_id=%d ORDER BY created_at DESC LIMIT 20",
        plant_id);

    if (mysql_query(conn, query) != 0) return -1;

    res = mysql_store_result(conn);
    if (!res) return -1;

    strcpy(out, "[");

    while ((row = mysql_fetch_row(res)) != NULL) {
        char item[512];
        snprintf(item, sizeof(item),
            "%s{\"id\":%s,\"plant_id\":%s,\"event_type\":\"%s\",\"message\":\"%s\",\"created_at\":\"%s\"}",
            first ? "" : ",", row[0], row[1], row[2], row[3], row[4]);

        if (!append_text(out, out_size, item)) {
            mysql_free_result(res);
            return -1;
        }
        first = 0;
    }

    if (!append_text(out, out_size, "]")) {
        mysql_free_result(res);
        return -1;
    }

    mysql_free_result(res);
    return 0;
}
