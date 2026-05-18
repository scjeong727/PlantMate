#include "db.h"
#include <stdio.h>

int db_ensure_schema(MYSQL* conn)
{
    const char* queries[] = {
        "CREATE TABLE IF NOT EXISTS mqtt_device_bindings ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "plant_id INT NOT NULL,"
        "role VARCHAR(31) NOT NULL,"
        "device_type VARCHAR(31) NOT NULL,"
        "device_id VARCHAR(63) NOT NULL,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP "
        "ON UPDATE CURRENT_TIMESTAMP,"
        "UNIQUE KEY uq_plant_role (plant_id, role)"
        ")",
        "CREATE TABLE IF NOT EXISTS mqtt_live_devices ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "device_type VARCHAR(31) NOT NULL,"
        "device_id VARCHAR(63) NOT NULL,"
        "online TINYINT(1) NOT NULL DEFAULT 0,"
        "status_payload VARCHAR(255) NOT NULL DEFAULT '',"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP "
        "ON UPDATE CURRENT_TIMESTAMP,"
        "UNIQUE KEY uq_device (device_type, device_id)"
        ")"
    };
    size_t i;

    if (!conn)
        return 0;

    for (i = 0; i < sizeof(queries) / sizeof(queries[0]); ++i) {
        if (mysql_query(conn, queries[i]) != 0) {
            fprintf(stderr, "db_ensure_schema: %s\n", mysql_error(conn));
            return 0;
        }
    }

    return 1;
}

int db_connect(MYSQL* conn)
{
    if (!conn) return 0;

    if (mysql_init(conn) == NULL) {
        fprintf(stderr, "db_connect: mysql_init failed\n");
        return 0;
    }

    if (!mysql_real_connect(
            conn,
            "127.0.0.1",
            "root",
            "1234",
            "plant_db",
            0,
            NULL,
            0)) {
        fprintf(stderr, "db_connect: %s\n", mysql_error(conn));
        return 0;
    }

    mysql_set_character_set(conn, "utf8");
    if (!db_ensure_schema(conn))
        return 0;
    return 1;
}

void db_close(MYSQL* conn)
{
    if (!conn) return;
    mysql_close(conn);
}
