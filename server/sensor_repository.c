#include "sensor_repository.h"
#include <stdio.h>
#include <string.h>

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

int sensor_repo_add(MYSQL* conn, int plant_id, double temp, double humi, int soil, int light)
{
    char query[512];

    if (!conn) return -1;

    snprintf(
        query,
        sizeof(query),
        "INSERT INTO sensor_data "
        "(plant_id, temp, humi, soil, light, created_at) "
        "VALUES (%d, %.1f, %.1f, %d, %d, NOW())",
        plant_id, temp, humi, soil, light
    );

    if (mysql_query(conn, query) != 0) {
        return -1;
    }

    return 0;
}

int sensor_repo_get_recent_json(MYSQL* conn, char* out, size_t out_size)
{
    const char* query =
        "SELECT id, plant_id, temp, humi, soil, light, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM sensor_data "
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
        "{\"id\":%s,\"plant_id\":%s,\"temp\":%s,\"humi\":%s,"
        "\"soil\":%s,\"light\":%s,\"created_at\":\"%s\"}",
        row[0], row[1], row[2], row[3], row[4], row[5], row[6]
    );

    mysql_free_result(res);
    return 0;
}

int sensor_repo_get_list_json(MYSQL* conn, char* out, size_t out_size)
{
    const char* query =
        "SELECT id, plant_id, temp, humi, soil, light, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM sensor_data "
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
        char item[256];

        snprintf(
            item,
            sizeof(item),
            "%s{\"id\":%s,\"plant_id\":%s,\"temp\":%s,\"humi\":%s,"
            "\"soil\":%s,\"light\":%s,\"created_at\":\"%s\"}",
            first ? "" : ",",
            row[0], row[1], row[2], row[3], row[4], row[5], row[6]
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

int sensor_repository_get_recent_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size)
{
    char query[256];
    MYSQL_RES* res;
    MYSQL_ROW row;

    if (!conn || !out) return -1;

    snprintf(query, sizeof(query),
        "SELECT id, plant_id, temp, humi, soil, light, created_at "
        "FROM sensor_data WHERE plant_id=%d ORDER BY created_at DESC LIMIT 1",
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
        "{\"id\":%s,\"plant_id\":%s,\"temp\":%s,\"humi\":%s,\"soil\":%s,\"light\":%s,\"created_at\":\"%s\"}",
        row[0], row[1], row[2], row[3], row[4], row[5], row[6]);

    mysql_free_result(res);
    return 0;
}

int sensor_repository_get_list_by_plant_json(MYSQL* conn, int plant_id, char* out, size_t out_size)
{
    char query[256];
    MYSQL_RES* res;
    MYSQL_ROW row;
    int first = 1;

    if (!conn || plant_id <= 0 || !out || out_size == 0)
        return -1;

    snprintf(
        query,
        sizeof(query),
        "SELECT id, plant_id, temp, humi, soil, light, "
        "DATE_FORMAT(created_at, '%%Y-%%m-%%d %%H:%%i:%%s') "
        "FROM sensor_data "
        "WHERE plant_id = %d "
        "ORDER BY id DESC LIMIT 50",
        plant_id
    );

    if (mysql_query(conn, query) != 0)
        return -1;

    res = mysql_store_result(conn);
    if (!res)
        return -1;

    out[0] = '\0';

    if (!append_text(out, out_size, "[")) {
        mysql_free_result(res);
        return -1;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        char item[256];

        snprintf(
            item,
            sizeof(item),
            "%s{\"id\":%s,\"plant_id\":%s,\"temp\":%s,\"humi\":%s,"
            "\"soil\":%s,\"light\":%s,\"created_at\":\"%s\"}",
            first ? "" : ",",
            row[0], row[1], row[2], row[3], row[4], row[5], row[6]
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
