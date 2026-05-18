#include "plant_repository.h"
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
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "INSERT INTO plants "
        "(user_id, name, type, temp_min, temp_max, humi_min, humi_max, soil_min, soil_max, light_min, light_max) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[11];
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
    bind[3].buffer = (char*)&temp_min;

    bind[4].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[4].buffer = (char*)&temp_max;

    bind[5].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[5].buffer = (char*)&humi_min;

    bind[6].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[6].buffer = (char*)&humi_max;

    bind[7].buffer_type = MYSQL_TYPE_LONG;
    bind[7].buffer = (char*)&soil_min;

    bind[8].buffer_type = MYSQL_TYPE_LONG;
    bind[8].buffer = (char*)&soil_max;

    bind[9].buffer_type = MYSQL_TYPE_LONG;
    bind[9].buffer = (char*)&light_min;

    bind[10].buffer_type = MYSQL_TYPE_LONG;
    bind[10].buffer = (char*)&light_max;

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
                row[4] ? row[4] : "0",
                row[5] ? row[5] : "0",
                row[6] ? row[6] : "0",
                row[7] ? row[7] : "0",
                row[8] ? row[8] : "0",
                row[9] ? row[9] : "0",
                row[10] ? row[10] : "0",
                row[11] ? row[11] : "0",
                row[12] ? row[12] : ""
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

        MYSQL_BIND result[13];
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
        result[4].buffer = (char*)&temp_min;

        result[5].buffer_type = MYSQL_TYPE_DOUBLE;
        result[5].buffer = (char*)&temp_max;

        result[6].buffer_type = MYSQL_TYPE_DOUBLE;
        result[6].buffer = (char*)&humi_min;

        result[7].buffer_type = MYSQL_TYPE_DOUBLE;
        result[7].buffer = (char*)&humi_max;

        result[8].buffer_type = MYSQL_TYPE_LONG;
        result[8].buffer = (char*)&soil_min;

        result[9].buffer_type = MYSQL_TYPE_LONG;
        result[9].buffer = (char*)&soil_max;

        result[10].buffer_type = MYSQL_TYPE_LONG;
        result[10].buffer = (char*)&light_min;

        result[11].buffer_type = MYSQL_TYPE_LONG;
        result[11].buffer = (char*)&light_max;

        result[12].buffer_type = MYSQL_TYPE_STRING;
        result[12].buffer = created_at;
        result[12].buffer_length = sizeof(created_at);
        result[12].length = &created_len;

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
                    char buf[1024];
                    snprintf(
                        buf, sizeof(buf),
                        "%s{\"plant_id\":%d,\"user_id\":%d,\"name\":\"%s\",\"type\":\"%s\","
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
    double temp_min, double temp_max,
    double humi_min, double humi_max,
    int soil_min, int soil_max,
    int light_min, int light_max)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "UPDATE plants "
        "SET name = ?, type = ?, "
        "temp_min = ?, temp_max = ?, "
        "humi_min = ?, humi_max = ?, "
        "soil_min = ?, soil_max = ?, "
        "light_min = ?, light_max = ? "
        "WHERE plant_id = ? AND user_id = ?";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[12];
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
    bind[2].buffer = (char*)&temp_min;

    bind[3].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[3].buffer = (char*)&temp_max;

    bind[4].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[4].buffer = (char*)&humi_min;

    bind[5].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[5].buffer = (char*)&humi_max;

    bind[6].buffer_type = MYSQL_TYPE_LONG;
    bind[6].buffer = (char*)&soil_min;

    bind[7].buffer_type = MYSQL_TYPE_LONG;
    bind[7].buffer = (char*)&soil_max;

    bind[8].buffer_type = MYSQL_TYPE_LONG;
    bind[8].buffer = (char*)&light_min;

    bind[9].buffer_type = MYSQL_TYPE_LONG;
    bind[9].buffer = (char*)&light_max;

    bind[10].buffer_type = MYSQL_TYPE_LONG;
    bind[10].buffer = (char*)&plant_id;

    bind[11].buffer_type = MYSQL_TYPE_LONG;
    bind[11].buffer = (char*)&user_id;

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
