#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>

int db_connect(MYSQL* conn);
void db_close(MYSQL* conn);
int db_ensure_schema(MYSQL* conn);

#endif
