#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include <mysql/mysql.h>

#include "command_queue.h"
#include "db.h"
#include "db_queue.h"
#include "event_log.h"
#include "mqtt_device_registry.h"
#include "plant_repository.h"
#include "watering_thread.h"
#include "watering_lock.h"
#include "device_lock.h"
#include "mqtt_adapter.h"
#include "ros2_bridge.h"

#define WATER_BAUDRATE B9600

extern DBQueue g_db_queue;
extern CommandQueue g_command_queue;
extern EventLog g_event_log;
extern DeviceLock g_water_device_lock;

static void build_water_bridge_detail(int plant_id, int duration, char* out, size_t out_size)
{
    MYSQL conn;
    double position_x = 0.0;
    double position_y = 0.0;
    int has_position = 0;

    if (!out || out_size == 0)
        return;

    snprintf(out, out_size, "{\"duration\":%d}", duration);

    if (!db_connect(&conn))
        return;

    if (plant_repository_get_position(&conn, plant_id, &position_x, &position_y, &has_position) &&
        has_position) {
        snprintf(out, out_size,
            "{\"duration\":%d,\"x\":%.3f,\"y\":%.3f}",
            duration, position_x, position_y);
    }

    mysql_close(&conn);
}

static int water_serial_open(const char* dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_SYNC);
    struct termios tio;

    if (fd < 0) return -1;
    if (tcgetattr(fd, &tio) != 0) { close(fd); return -1; }

    cfsetispeed(&tio, WATER_BAUDRATE);
    cfsetospeed(&tio, WATER_BAUDRATE);

    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;
    tio.c_iflag &= ~IGNBRK;
    tio.c_lflag = 0;
    tio.c_oflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~(PARENB | PARODD);
    tio.c_cflag &= ~CSTOPB;
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif

    if (tcsetattr(fd, TCSANOW, &tio) != 0) { close(fd); return -1; }
    tcflush(fd, TCIOFLUSH);
    usleep(2000000);
    return fd;
}

static int water_send_command(int fd, int duration)
{
    char tx[64];
    char rx[64];
    int len, n, pos = 0;

    if (fd < 0 || duration <= 0) return 0;

    tcflush(fd, TCIOFLUSH);
    len = snprintf(tx, sizeof(tx), "WATER %d\n", duration);
    if (write(fd, tx, len) != len) return 0;
    tcdrain(fd);

    memset(rx, 0, sizeof(rx));
    while (pos < (int)sizeof(rx) - 1) {
        n = read(fd, rx + pos, 1);
        if (n > 0) {
            if (rx[pos] == '\n') break;
            pos += n;
        } else {
            break;
        }
    }
    rx[pos] = '\0';
    printf("water board reply: [%s]\n", rx);

    if (strstr(rx, "OK") != NULL) return 1;
    return 0;
}


void* watering_thread_main(void* arg)
{
    WaterCommand cmd;
    (void)arg;

    while (1) {
        int ok = 0;
        int dispatched_to_mqtt_device = 0;
        int dispatched_to_ros2 = 0;
        int serial_fd = -1;
        char device_path[256];
        DBRequest req;
        MqttDeviceBinding mqtt_binding;

        command_queue_pop(&g_command_queue, &cmd);
        printf("watering start: plant_id=%d duration=%d owner=%d\n",
               cmd.plant_id, cmd.duration, cmd.owner_sock);

        memset(device_path, 0, sizeof(device_path));

        if (!device_lock_get_device_by_owner(&g_water_device_lock, cmd.owner_sock, device_path, sizeof(device_path))) {
            if (mqtt_device_registry_get(cmd.plant_id, "water", &mqtt_binding)) {
                char detail[128];
                build_water_bridge_detail(cmd.plant_id, cmd.duration, detail, sizeof(detail));
                mqtt_adapter_publish_bridge_command(cmd.plant_id, "water", detail);
                dispatched_to_mqtt_device = 1;
                ok = 1;
                printf("watering delegated to mqtt ros bridge: type=%s id=%s\n",
                    mqtt_binding.device_type, mqtt_binding.device_id);
            } else {
                char detail[128];
                build_water_bridge_detail(cmd.plant_id, cmd.duration, detail, sizeof(detail));
                if (ros2_bridge_publish_command(cmd.plant_id, "water", detail)) {
                    dispatched_to_ros2 = 1;
                    ok = 1;
                    printf("watering delegated to ros2 topic\n");
                } else {
                    printf("watering device path not found\n");
                }
            }
        } else {
            serial_fd = water_serial_open(device_path);
            if (serial_fd < 0) {
                perror("water_serial_open");
            }
        }

        if (!dispatched_to_mqtt_device && cmd.duration > 0 && serial_fd >= 0) {
            ok = water_send_command(serial_fd, cmd.duration);
        }

        if (serial_fd >= 0) {
            close(serial_fd);
        }

        printf("watering end: plant_id=%d, ok=%d\n", cmd.plant_id, ok);

        if (dispatched_to_mqtt_device || dispatched_to_ros2) {
            event_log_push(&g_event_log, cmd.plant_id, "WATERING_SENT", "Watering_command_published");
            mqtt_adapter_publish_status(cmd.plant_id, "{\"eventType\":\"WATERING_SENT\",\"message\":\"Watering_command_published\"}");
        } else if (ok) {
            event_log_push(&g_event_log, cmd.plant_id, "WATERING_DONE", "Watering_completed");
            mqtt_adapter_publish_status(cmd.plant_id, "{\"eventType\":\"WATERING_DONE\",\"message\":\"Watering_completed\"}");
        } else {
            event_log_push(&g_event_log, cmd.plant_id, "WATERING_FAIL", "Watering_failed");
            mqtt_adapter_publish_status(cmd.plant_id, "{\"eventType\":\"WATERING_FAIL\",\"message\":\"Watering_failed\"}");
        }

        memset(&req, 0, sizeof(req));
        req.type = DB_REQ_SENSOR;
        req.client_sock = -1;

        snprintf(req.query, DB_QUERY_SIZE,
                 "INSERT_EVENT %d %s %s",
                 cmd.plant_id,
                 (dispatched_to_mqtt_device || dispatched_to_ros2) ? "WATERING_SENT" : (ok ? "WATERING_DONE" : "WATERING_FAIL"),
                 (dispatched_to_mqtt_device || dispatched_to_ros2) ? "Watering_command_published" : (ok ? "Watering_completed" : "Watering_failed"));

        db_queue_push(&g_db_queue, &req);
        watering_end(cmd.plant_id);
    }

    return NULL;
}
