#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdint.h>

#include "db_queue.h"
#include "sensor_buffer.h"
#include "event_log.h"
#include "sensor_thread.h"
#include "plant_threshold_cache.h"
#include "sensor_event_dispatcher.h"
#include "mqtt_adapter.h"
#include "server_config.h"

extern DBQueue g_db_queue;
extern SensorBuffer g_sensor_buffer;
extern EventLog g_event_log;

#define BUF_SIZE 4096

static void push_db_req(int client_sock, const char* text)
{
    DBRequest req;
    memset(&req, 0, sizeof(req));
    req.type = DB_REQ_SENSOR;
    req.client_sock = client_sock;
    snprintf(req.query, DB_QUERY_SIZE, "%s", text);
    db_queue_push(&g_db_queue, &req);
}

static void notify_sensor_event_status(int plant_id, const char* message)
{
    mqtt_adapter_publish_status(plant_id, message);
}

void* sensor_thread_main(void* arg)
{
    (void)arg;

    int s, c;
    int port = server_config_get()->sensor_port;
    struct sockaddr_in a, ca;
    socklen_t l;
    char buf[BUF_SIZE];

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        perror("socket");
        return NULL;
    }

    {
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons((uint16_t)port);

    if (bind(s, (struct sockaddr*)&a, sizeof(a)) == -1) {
        perror("bind");
        close(s);
        return NULL;
    }

    if (listen(s, 5) == -1) {
        perror("listen");
        close(s);
        return NULL;
    }

    printf("sensor thread listening on %d\n", port);

    while (1)
    {
        l = sizeof(ca);
        c = accept(s, (struct sockaddr*)&ca, &l);
        if (c == -1) continue;

        while (1)
        {
            int n = recv(c, buf, BUF_SIZE - 1, 0);
            int plant_id, soil, light;
            double temp, humi;
            Reading r;
            char mqtt_payload[256];
            time_t now;
            struct tm tm_now;

            if (n <= 0) break;
            buf[n] = '\0';

            if (sscanf(buf, "POST_SENSOR_DATA %d %lf %lf %d %d",
                       &plant_id, &temp, &humi, &soil, &light) != 5) {
                continue;
            }

            memset(&r, 0, sizeof(r));
            r.plant_id = plant_id;
            r.ts_ms = (uint64_t)time(NULL) * 1000;
            r.temp_c = temp;
            r.humi_pct = humi;
            r.soil_raw = soil;
            r.cds_raw = light;

            now = time(NULL);
            localtime_r(&now, &tm_now);
            strftime(r.created_at, sizeof(r.created_at), "%Y-%m-%d %H:%M:%S", &tm_now);

            sensor_buffer_push(&g_sensor_buffer, &r);

            sensor_event_dispatcher_evaluate(c, plant_id, temp, humi, soil, light, notify_sensor_event_status);
            snprintf(mqtt_payload, sizeof(mqtt_payload),
                "{\"temperature\":%.2f,\"humidity\":%.2f,\"soilMoisture\":%d,\"light\":%d,\"timestamp\":\"%s\"}",
                temp, humi, soil, light, r.created_at);
            mqtt_adapter_publish_sensor(plant_id, mqtt_payload);

            push_db_req(c, buf);
        }

        close(c);
    }

    close(s);
    return NULL;
}
