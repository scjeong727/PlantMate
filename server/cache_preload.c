#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include "db.h"
#include "sensor_buffer.h"
#include "event_log.h"
#include "plant_owner_cache.h"
#include "plant_threshold_cache.h"

extern SensorBuffer g_sensor_buffer;
extern EventLog g_event_log;

int cache_preload_all(void)
{
    MYSQL conn;
    MYSQL_RES* res;
    MYSQL_ROW row;
    Reading readings[128];
    EventLogItem events[128];
    int rcnt = 0, ecnt = 0, i;

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn)) {
        printf("cache preload: db_connect failed\n");
        return 0;
    }

    plant_threshold_cache_clear();

    if (mysql_query(&conn,
        "SELECT plant_id, temp_min, temp_max, humi_min, humi_max, "
        "soil_min, soil_max, light_min, light_max "
        "FROM plants") == 0) {
        res = mysql_store_result(&conn);
        if (res) {
            while ((row = mysql_fetch_row(res)) != NULL) {
                plant_threshold_cache_set(
                    atoi(row[0]),
                    atof(row[1]), atof(row[2]),
                    atof(row[3]), atof(row[4]),
                    atoi(row[5]), atoi(row[6]),
                    atoi(row[7]), atoi(row[8])
                );
            }
            mysql_free_result(res);
        }
    }

    if (mysql_query(&conn,
        "SELECT id, plant_id, temp, humi, soil, light, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM sensor_data ORDER BY id DESC LIMIT 128") == 0) {
        res = mysql_store_result(&conn);
        if (res) {
            while ((row = mysql_fetch_row(res)) != NULL && rcnt < 128) {
                readings[rcnt].id = atoi(row[0]);
                readings[rcnt].plant_id = atoi(row[1]);
                readings[rcnt].temp_c = atof(row[2]);
                readings[rcnt].humi_pct = atof(row[3]);
                readings[rcnt].soil_raw = atoi(row[4]);
                readings[rcnt].cds_raw = atoi(row[5]);
                readings[rcnt].ts_ms = 0;
                snprintf(readings[rcnt].created_at, sizeof(readings[rcnt].created_at), "%s", row[6]);
                rcnt++;
            }
            mysql_free_result(res);
        }
    }

    if (mysql_query(&conn,
        "SELECT id, plant_id, event_type, message, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM events ORDER BY id DESC LIMIT 128") == 0) {
        res = mysql_store_result(&conn);
        if (res) {
            while ((row = mysql_fetch_row(res)) != NULL && ecnt < 128) {
                events[ecnt].id = atoi(row[0]);
                events[ecnt].plant_id = atoi(row[1]);
                snprintf(events[ecnt].event_type, sizeof(events[ecnt].event_type), "%s", row[2]);
                snprintf(events[ecnt].message, sizeof(events[ecnt].message), "%s", row[3]);
                snprintf(events[ecnt].created_at, sizeof(events[ecnt].created_at), "%s", row[4]);
                ecnt++;
            }
            mysql_free_result(res);
        }
    }

    for (i = rcnt - 1; i >= 0; --i)
        sensor_buffer_push_loaded(&g_sensor_buffer, &readings[i]);

    for (i = ecnt - 1; i >= 0; --i)
        event_log_push_item(&g_event_log, &events[i]);

    db_close(&conn);
    printf("cache preload done: sensor=%d, event=%d\n", rcnt, ecnt);
    return 1;
}

int plant_owner_cache_preload_all(void)
{
    MYSQL conn;
    MYSQL_RES* res;
    MYSQL_ROW row;

    memset(&conn, 0, sizeof(conn));
    if (!db_connect(&conn)) {
        printf("plant owner cache preload: db_connect failed\n");
        return 0;
    }

    if (mysql_query(&conn, "SELECT plant_id, user_id FROM plants") != 0) {
        db_close(&conn);
        return 0;
    }

    res = mysql_store_result(&conn);
    if (!res) {
        db_close(&conn);
        return 0;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        plant_owner_cache_set(atoi(row[0]), atoi(row[1]));
    }

    mysql_free_result(res);
    db_close(&conn);
    return 1;
}
