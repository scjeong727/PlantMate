#include "user_repository.h"
#include <stdio.h>
#include <string.h>

static void append_text(char* out, size_t out_size, const char* text)
{
    size_t cur = strlen(out);
    if (cur >= out_size - 1) return;
    snprintf(out + cur, out_size - cur, "%s", text);
}

int user_repository_add(MYSQL* conn, const char* login_id, const char* password, const char* name)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "INSERT INTO users (login_id, password_hash, name) "
        "VALUES (?, ?, ?)";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    unsigned long login_len = (unsigned long)strlen(login_id);
    unsigned long pass_len  = (unsigned long)strlen(password);
    unsigned long name_len  = (unsigned long)strlen(name);

    bind[0].buffer_type   = MYSQL_TYPE_STRING;
    bind[0].buffer        = (char*)login_id;
    bind[0].buffer_length = login_len;
    bind[0].length        = &login_len;

    bind[1].buffer_type   = MYSQL_TYPE_STRING;
    bind[1].buffer        = (char*)password;
    bind[1].buffer_length = pass_len;
    bind[1].length        = &pass_len;

    bind[2].buffer_type   = MYSQL_TYPE_STRING;
    bind[2].buffer        = (char*)name;
    bind[2].buffer_length = name_len;
    bind[2].length        = &name_len;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    int ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}

int user_repository_get_all(MYSQL* conn, char* out, size_t out_size)
{
    const char* sql =
        "SELECT user_id, login_id, name, created_at "
        "FROM users ORDER BY user_id ASC";

    if (mysql_query(conn, sql) != 0) return 0;

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return 0;

    MYSQL_ROW row;
    out[0] = '\0';
    append_text(out, out_size, "[");

    int first = 1;
    while ((row = mysql_fetch_row(res)) != NULL) {
        char buf[512];
        snprintf(
            buf, sizeof(buf),
            "%s{\"user_id\":%s,\"login_id\":\"%s\",\"name\":\"%s\",\"created_at\":\"%s\"}",
            first ? "" : ",",
            row[0] ? row[0] : "0",
            row[1] ? row[1] : "",
            row[2] ? row[2] : "",
            row[3] ? row[3] : ""
        );
        append_text(out, out_size, buf);
        first = 0;
    }

    append_text(out, out_size, "]");
    mysql_free_result(res);
    return 1;
}

int user_repository_remove(MYSQL* conn, int user_id)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql = "DELETE FROM users WHERE user_id = ?";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    int ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}

int user_repository_login(MYSQL* conn, const char* login_id, const char* password, int* out_user_id)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "SELECT user_id "
        "FROM users "
        "WHERE login_id = ? AND password_hash = ? "
        "LIMIT 1";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND param[2];
    memset(param, 0, sizeof(param));

    unsigned long login_len = (unsigned long)strlen(login_id);
    unsigned long pass_len  = (unsigned long)strlen(password);

    param[0].buffer_type   = MYSQL_TYPE_STRING;
    param[0].buffer        = (char*)login_id;
    param[0].buffer_length = login_len;
    param[0].length        = &login_len;

    param[1].buffer_type   = MYSQL_TYPE_STRING;
    param[1].buffer        = (char*)password;
    param[1].buffer_length = pass_len;
    param[1].length        = &pass_len;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    int user_id = 0;
    MYSQL_BIND result[1];
    memset(result, 0, sizeof(result));

    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = (char*)&user_id;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    int fetch_ret = mysql_stmt_fetch(stmt);
    if (fetch_ret == 0 || fetch_ret == MYSQL_DATA_TRUNCATED) {
        *out_user_id = user_id;
        mysql_stmt_close(stmt);
        return 1;
    }

    mysql_stmt_close(stmt);
    return 0;
}

int user_repository_find_id_by_login_id(MYSQL* conn, const char* login_id, int* out_user_id)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char* sql =
        "SELECT user_id "
        "FROM users "
        "WHERE login_id = ? "
        "LIMIT 1";

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND param[1];
    MYSQL_BIND result[1];
    unsigned long login_len;
    int user_id = 0;

    memset(param, 0, sizeof(param));
    memset(result, 0, sizeof(result));

    login_len = (unsigned long)strlen(login_id);
    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = (char*)login_id;
    param[0].buffer_length = login_len;
    param[0].length = &login_len;

    if (mysql_stmt_bind_param(stmt, param) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = (char*)&user_id;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_fetch(stmt) == 0) {
        *out_user_id = user_id;
        mysql_stmt_close(stmt);
        return 1;
    }

    mysql_stmt_close(stmt);
    return 0;
}
