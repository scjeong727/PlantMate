#include "plant_repository.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void append_text(char* out, size_t out_size, const char* text)
{
    size_t cur = strlen(out);
    if (cur >= out_size - 1) return;
    snprintf(out + cur, out_size - cur, "%s", text);
}

int plant_repository_add(
    MYSQL* conn,
    int user_id,
    const char* name,
    const char* type,
    int has_position_x, double position_x,
    int has_position_y, double position_y,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "INSERT INTO plants "
        "(user_id, name, type, position_x, position_y, temp_min, temp_max, humi_min, humi_max, soil_min, soil_max, light_min, light_max) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[13];
    bool position_x_is_null = !has_position_x;
    bool position_y_is_null = !has_position_y;
    memset(bind, 0, sizeof(bind));

    unsigned long name_len = (unsigned long)strlen(name);
    unsigned long type_len = (unsigned long)strlen(type);

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)name;
    bind[1].buffer_length = name_len;
    bind[1].length = &name_len;

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)type;
    bind[2].buffer_length = type_len;
    bind[2].length = &type_len;

    bind[3].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[3].buffer = (char*)&position_x;
    bind[3].is_null = &position_x_is_null;

    bind[4].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[4].buffer = (char*)&position_y;
    bind[4].is_null = &position_y_is_null;

    bind[5].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[5].buffer = (char*)&temp_min;

    bind[6].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[6].buffer = (char*)&temp_max;

    bind[7].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[7].buffer = (char*)&humi_min;

    bind[8].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[8].buffer = (char*)&humi_max;

    bind[9].buffer_type = MYSQL_TYPE_LONG;
    bind[9].buffer = (char*)&soil_min;

    bind[10].buffer_type = MYSQL_TYPE_LONG;
    bind[10].buffer = (char*)&soil_max;

    bind[11].buffer_type = MYSQL_TYPE_LONG;
    bind[11].buffer = (char*)&light_min;

    bind[12].buffer_type = MYSQL_TYPE_LONG;
    bind[12].buffer = (char*)&light_max;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    {
        int ok = (mysql_stmt_execute(stmt) == 0);
        mysql_stmt_close(stmt);
        return ok;
    }
}

int plant_repository_get_all(MYSQL* conn, char* out, size_t out_size)
{
    const char* sql =
        "SELECT plant_id, user_id, name, type, "
        "position_x, position_y, "
        "temp_min, temp_max, humi_min, humi_max, soil_min, soil_max, light_min, light_max, "
        "created_at "
        "FROM plants ORDER BY plant_id ASC";

    if (mysql_query(conn, sql) != 0) return 0;

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return 0;

    MYSQL_ROW row;
    out[0] = '\0';
    append_text(out, out_size, "[");

    {
        int first = 1;
        while ((row = mysql_fetch_row(res)) != NULL) {
            char buf[1024];
            snprintf(
                buf, sizeof(buf),
                "%s{\"plant_id\":%s,\"user_id\":%s,\"name\":\"%s\",\"type\":\"%s\","
                "\"position_x\":%s,\"position_y\":%s,"
                "\"temp_min\":%s,\"temp_max\":%s,"
                "\"humi_min\":%s,\"humi_max\":%s,"
                "\"soil_min\":%s,\"soil_max\":%s,"
                "\"light_min\":%s,\"light_max\":%s,"
                "\"created_at\":\"%s\"}",
                first ? "" : ",",
                row[0] ? row[0] : "0",
                row[1] ? row[1] : "0",
                row[2] ? row[2] : "",
                row[3] ? row[3] : "",
                row[4] ? row[4] : "null",
                row[5] ? row[5] : "null",
                row[6] ? row[6] : "0",
                row[7] ? row[7] : "0",
                row[8] ? row[8] : "0",
                row[9] ? row[9] : "0",
                row[10] ? row[10] : "0",
                row[11] ? row[11] : "0",
                row[12] ? row[12] : "0",
                row[13] ? row[13] : "0",
                row[14] ? row[14] : ""
            );
            append_text(out, out_size, buf);
            first = 0;
        }
    }

    append_text(out, out_size, "]");
    mysql_free_result(res);
    return 1;
}

int plant_repository_get_by_user(MYSQL* conn, int user_id, char* out, size_t out_size)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "SELECT plant_id, user_id, name, type, "
        "position_x, position_y, "
        "temp_min, temp_max, humi_min, humi_max, soil_min, soil_max, light_min, light_max, "
        "created_at "
        "FROM plants "
        "WHERE user_id = ? "
        "ORDER BY plant_id ASC";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND param[1];
    memset(param, 0, sizeof(param));
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&user_id;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    {
        int plant_id_val = 0;
        int user_id_val = 0;
        char name[128] = {0};
        char type[128] = {0};
        double position_x = 0;
        double position_y = 0;
        double temp_min = 0;
        double temp_max = 0;
        double humi_min = 0;
        double humi_max = 0;
        int soil_min = 0;
        int soil_max = 0;
        int light_min = 0;
        int light_max = 0;
        char created_at[64] = {0};

        unsigned long name_len = 0;
        unsigned long type_len = 0;
        unsigned long created_len = 0;
        bool position_x_is_null = false;
        bool position_y_is_null = false;

        MYSQL_BIND result[15];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = (char*)&plant_id_val;

        result[1].buffer_type = MYSQL_TYPE_LONG;
        result[1].buffer = (char*)&user_id_val;

        result[2].buffer_type = MYSQL_TYPE_STRING;
        result[2].buffer = name;
        result[2].buffer_length = sizeof(name);
        result[2].length = &name_len;

        result[3].buffer_type = MYSQL_TYPE_STRING;
        result[3].buffer = type;
        result[3].buffer_length = sizeof(type);
        result[3].length = &type_len;

        result[4].buffer_type = MYSQL_TYPE_DOUBLE;
        result[4].buffer = (char*)&position_x;
        result[4].is_null = &position_x_is_null;

        result[5].buffer_type = MYSQL_TYPE_DOUBLE;
        result[5].buffer = (char*)&position_y;
        result[5].is_null = &position_y_is_null;

        result[6].buffer_type = MYSQL_TYPE_DOUBLE;
        result[6].buffer = (char*)&temp_min;

        result[7].buffer_type = MYSQL_TYPE_DOUBLE;
        result[7].buffer = (char*)&temp_max;

        result[8].buffer_type = MYSQL_TYPE_DOUBLE;
        result[8].buffer = (char*)&humi_min;

        result[9].buffer_type = MYSQL_TYPE_DOUBLE;
        result[9].buffer = (char*)&humi_max;

        result[10].buffer_type = MYSQL_TYPE_LONG;
        result[10].buffer = (char*)&soil_min;

        result[11].buffer_type = MYSQL_TYPE_LONG;
        result[11].buffer = (char*)&soil_max;

        result[12].buffer_type = MYSQL_TYPE_LONG;
        result[12].buffer = (char*)&light_min;

        result[13].buffer_type = MYSQL_TYPE_LONG;
        result[13].buffer = (char*)&light_max;

        result[14].buffer_type = MYSQL_TYPE_STRING;
        result[14].buffer = created_at;
        result[14].buffer_length = sizeof(created_at);
        result[14].length = &created_len;

        if (mysql_stmt_bind_result(stmt, result) != 0) {
            mysql_stmt_close(stmt);
            return 0;
        }

        if (mysql_stmt_store_result(stmt) != 0) {
            mysql_stmt_close(stmt);
            return 0;
        }

        out[0] = '\0';
        append_text(out, out_size, "[");

        {
            int first = 1;
            while (1) {
                int fr = mysql_stmt_fetch(stmt);
                if (fr == MYSQL_NO_DATA) break;
                if (fr != 0 && fr != MYSQL_DATA_TRUNCATED) {
                    mysql_stmt_close(stmt);
                    return 0;
                }

                name[name_len < sizeof(name) ? name_len : sizeof(name) - 1] = '\0';
                type[type_len < sizeof(type) ? type_len : sizeof(type) - 1] = '\0';
                created_at[created_len < sizeof(created_at) ? created_len : sizeof(created_at) - 1] = '\0';

                {
                    char position_x_text[32];
                    char position_y_text[32];
                    char buf[1024];

                    if (position_x_is_null)
                        snprintf(position_x_text, sizeof(position_x_text), "null");
                    else
                        snprintf(position_x_text, sizeof(position_x_text), "%.2f", position_x);

                    if (position_y_is_null)
                        snprintf(position_y_text, sizeof(position_y_text), "null");
                    else
                        snprintf(position_y_text, sizeof(position_y_text), "%.2f", position_y);

                    snprintf(
                        buf, sizeof(buf),
                        "%s{\"plant_id\":%d,\"user_id\":%d,\"name\":\"%s\",\"type\":\"%s\","
                        "\"position_x\":%s,\"position_y\":%s,"
                        "\"temp_min\":%.2f,\"temp_max\":%.2f,"
                        "\"humi_min\":%.2f,\"humi_max\":%.2f,"
                        "\"soil_min\":%d,\"soil_max\":%d,"
                        "\"light_min\":%d,\"light_max\":%d,"
                        "\"created_at\":\"%s\"}",
                        first ? "" : ",",
                        plant_id_val,
                        user_id_val,
                        name,
                        type,
                        position_x_text,
                        position_y_text,
                        temp_min, temp_max,
                        humi_min, humi_max,
                        soil_min, soil_max,
                        light_min, light_max,
                        created_at
                    );
                    append_text(out, out_size, buf);
                }
                first = 0;
            }
        }

        append_text(out, out_size, "]");
    }

    mysql_stmt_close(stmt);
    return 1;
}

int plant_repository_remove(MYSQL* conn, int plant_id, int user_id)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql = "DELETE FROM plants WHERE plant_id = ? AND user_id = ?";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&plant_id;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&user_id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    {
        my_ulonglong affected = mysql_stmt_affected_rows(stmt);
        mysql_stmt_close(stmt);
        return (affected > 0) ? 1 : 0;
    }
}

int plant_repository_edit(
    MYSQL* conn,
    int plant_id,
    int user_id,
    const char* name,
    const char* type,
    int has_position_x, double position_x,
    int has_position_y, double position_y,
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "UPDATE plants "
        "SET name = ?, type = ?, position_x = ?, position_y = ?, "
        "temp_min = ?, temp_max = ?, "
        "humi_min = ?, humi_max = ?, "
        "soil_min = ?, soil_max = ?, "
        "light_min = ?, light_max = ? "
        "WHERE plant_id = ? AND user_id = ?";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[14];
    bool position_x_is_null = !has_position_x;
    bool position_y_is_null = !has_position_y;
    memset(bind, 0, sizeof(bind));

    unsigned long name_len = (unsigned long)strlen(name);
    unsigned long type_len = (unsigned long)strlen(type);

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)name;
    bind[0].buffer_length = name_len;
    bind[0].length = &name_len;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)type;
    bind[1].buffer_length = type_len;
    bind[1].length = &type_len;

    bind[2].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[2].buffer = (char*)&position_x;
    bind[2].is_null = &position_x_is_null;

    bind[3].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[3].buffer = (char*)&position_y;
    bind[3].is_null = &position_y_is_null;

    bind[4].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[4].buffer = (char*)&temp_min;

    bind[5].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[5].buffer = (char*)&temp_max;

    bind[6].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[6].buffer = (char*)&humi_min;

    bind[7].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[7].buffer = (char*)&humi_max;

    bind[8].buffer_type = MYSQL_TYPE_LONG;
    bind[8].buffer = (char*)&soil_min;

    bind[9].buffer_type = MYSQL_TYPE_LONG;
    bind[9].buffer = (char*)&soil_max;

    bind[10].buffer_type = MYSQL_TYPE_LONG;
    bind[10].buffer = (char*)&light_min;

    bind[11].buffer_type = MYSQL_TYPE_LONG;
    bind[11].buffer = (char*)&light_max;

    bind[12].buffer_type = MYSQL_TYPE_LONG;
    bind[12].buffer = (char*)&plant_id;

    bind[13].buffer_type = MYSQL_TYPE_LONG;
    bind[13].buffer = (char*)&user_id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    {
        my_ulonglong affected = mysql_stmt_affected_rows(stmt);
        mysql_stmt_close(stmt);
        return (affected > 0) ? 1 : 0;
    }
}

int plant_repository_exists_by_user(MYSQL* conn, int plant_id, int user_id)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "SELECT COUNT(*) "
        "FROM plants "
        "WHERE plant_id = ? AND user_id = ?";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND param[2];
    memset(param, 0, sizeof(param));

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*)&plant_id;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = (char*)&user_id;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    {
        int count = 0;
        MYSQL_BIND result[1];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = (char*)&count;

        if (mysql_stmt_bind_result(stmt, result) != 0) {
            mysql_stmt_close(stmt);
            return 0;
        }

        if (mysql_stmt_store_result(stmt) != 0) {
            mysql_stmt_close(stmt);
            return 0;
        }

        if (mysql_stmt_fetch(stmt) != 0) {
            mysql_stmt_close(stmt);
            return 0;
        }

        mysql_stmt_close(stmt);
        return (count > 0) ? 1 : 0;
    }
}

int plant_repository_get_owner_user_id(MYSQL* conn, int plant_id)
{
    char query[256];
    MYSQL_RES* res;
    MYSQL_ROW row;
    int user_id = -1;

    snprintf(query, sizeof(query),
        "SELECT user_id FROM plants WHERE plant_id=%d LIMIT 1",
        plant_id);

    if (mysql_query(conn, query) != 0)
        return -1;

    res = mysql_store_result(conn);
    if (!res)
        return -1;

    row = mysql_fetch_row(res);
    if (row && row[0])
        user_id = atoi(row[0]);

    mysql_free_result(res);
    return user_id;
}

int plant_repository_remove_sensor_data_by_plant(MYSQL* conn, int plant_id)
{
    char query[256];

    if (!conn || plant_id <= 0) return 0;

    snprintf(query, sizeof(query),
        "DELETE FROM sensor_data WHERE plant_id=%d", plant_id);

    return (mysql_query(conn, query) == 0);
}

int plant_repository_remove_events_by_plant(MYSQL* conn, int plant_id)
{
    char query[256];

    if (!conn || plant_id <= 0) return 0;

    snprintf(query, sizeof(query),
        "DELETE FROM events WHERE plant_id=%d", plant_id);

    return (mysql_query(conn, query) == 0);
}

int plant_repository_get_position(
    MYSQL* conn,
    int plant_id,
    double* position_x,
    double* position_y,
    int* has_position)
{
    char query[256];
    MYSQL_RES* res;
    MYSQL_ROW row;

    if (!conn || plant_id <= 0 || !position_x || !position_y || !has_position)
        return 0;

    *position_x = 0.0;
    *position_y = 0.0;
    *has_position = 0;

    snprintf(query, sizeof(query),
        "SELECT position_x, position_y FROM plants WHERE plant_id=%d LIMIT 1",
        plant_id);

    if (mysql_query(conn, query) != 0)
        return 0;

    res = mysql_store_result(conn);
    if (!res)
        return 0;

    row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return 0;
    }

    if (row[0] && row[1]) {
        *position_x = atof(row[0]);
        *position_y = atof(row[1]);
        *has_position = 1;
    }

    mysql_free_result(res);
    return 1;
}
