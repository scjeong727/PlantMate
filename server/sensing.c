#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "sensing.h"
#include "device_lock.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define SENSING_SERIAL_DEVICE "/dev/ttyACM0"
#define WATERING_SERIAL_DEVICE "/dev/ttyACM1"
#define SENSING_BAUDRATE B9600
#define SENSING_SERVER_IP "127.0.0.1"
#define SENSING_SERVER_PORT 9001
#define SENSING_INTERVAL_SEC 5

#define LINE_BUF_SIZE 256
#define ACC_BUF_SIZE 512

extern DeviceLock g_sensor_device_lock;

typedef struct SensorReading {
    double temp;
    double humi;
    int soil;
    int light;
} SensorReading;

typedef struct SensorRuntime {
    int fd;
    int plant_id;
    char current_device[256];
    char acc[ACC_BUF_SIZE];
    size_t acc_len;
    SensorReading latest;
    int has_latest;
    time_t last_sent;
} SensorRuntime;

static int serial_open_nonblock(const char* dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    struct termios tio;

    if (fd < 0) return -1;
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return -1;
    }

    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    cfsetispeed(&tio, SENSING_BAUDRATE);
    cfsetospeed(&tio, SENSING_BAUDRATE);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

static int serial_readline_nonblock(int fd, char* out, size_t cap, char* acc, size_t* alen)
{
    char tmp[128];

    if (!out || cap == 0 || !acc || !alen)
        return -1;

    for (;;) {
        ssize_t r = read(fd, tmp, sizeof(tmp));

        if (r > 0) {
            ssize_t i;

            for (i = 0; i < r; ++i) {
                char c = tmp[i];

                if (c == '\r')
                    continue;

                if (*alen < ACC_BUF_SIZE - 1)
                    acc[(*alen)++] = c;

                if (c == '\n') {
                    size_t n;

                    acc[*alen] = '\0';
                    n = (*alen < cap - 1) ? *alen : cap - 1;
                    memcpy(out, acc, n);
                    out[n] = '\0';

                    if (n > 0 && out[n - 1] == '\n')
                        out[n - 1] = '\0';

                    *alen = 0;
                    return 1;
                }
            }

            continue;
        }

        if (r == 0)
            return 0;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return -1;
    }
}

static int find_active_index(ActiveSensorInfo* items, int count, int index)
{
    int i;

    for (i = 0; i < count; ++i) {
        if (items[i].index == index)
            return i;
    }

    return -1;
}

static bool parse_line(const char* line, SensorReading* out)
{
    unsigned long long ts = 0;
    double temp = 0.0;
    double humi = 0.0;
    int soil = 0;
    int light = 0;

    if (!line || !out) return false;

    if (sscanf(line, " %llu , %lf , %lf , %d , %d", &ts, &temp, &humi, &soil, &light) == 5) {
        out->temp = temp;
        out->humi = humi;
        out->soil = soil;
        out->light = light;
        return true;
    }

    if (sscanf(line, " %lf , %lf , %d , %d", &temp, &humi, &soil, &light) == 4) {
        out->temp = temp;
        out->humi = humi;
        out->soil = soil;
        out->light = light;
        return true;
    }

    return false;
}

static int send_to_sensor_thread(int plant_id, const SensorReading* reading)
{
    int sock;
    int n;
    char req[256];
    char resp[256];
    struct sockaddr_in serv_addr;

    if (!reading) return 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SENSING_SERVER_PORT);
    if (inet_pton(AF_INET, SENSING_SERVER_IP, &serv_addr.sin_addr) <= 0) {
        close(sock);
        return 0;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return 0;
    }

    snprintf(req, sizeof(req), "POST_SENSOR_DATA %d %.2f %.2f %d %d\n",
             plant_id,
             reading->temp,
             reading->humi,
             reading->soil,
             reading->light);

    if (send(sock, req, strlen(req), 0) < 0) {
        close(sock);
        return 0;
    }

    n = recv(sock, resp, sizeof(resp) - 1, 0);
    if (n > 0) {
        resp[n] = '\0';
        printf("[sensing] %s", resp);
    }

    close(sock);
    return 1;
}

void* sensing_thread_main(void* arg)
{
    SensorRuntime states[MAX_DEVICE_LOCKS];
    ActiveSensorInfo active_items[MAX_DEVICE_LOCKS];
    char line[LINE_BUF_SIZE];
    SensorReading current;
    int i;

    (void)arg;
    memset(states, 0, sizeof(states));

    for (i = 0; i < MAX_DEVICE_LOCKS; ++i) {
        states[i].fd = -1;
        states[i].plant_id = -1;
    }

    while (1) {
        int active_count = device_lock_get_active_sensor_list(
            &g_sensor_device_lock,
            active_items,
            MAX_DEVICE_LOCKS);

        for (i = 0; i < MAX_DEVICE_LOCKS; ++i) {
            int active_pos = find_active_index(active_items, active_count, i);
            SensorRuntime* st = &states[i];

            if (active_pos < 0) {
                if (st->fd >= 0) {
                    close(st->fd);
                    st->fd = -1;
                }

                st->current_device[0] = '\0';
                st->acc_len = 0;
                st->has_latest = 0;
                st->last_sent = 0;
                st->plant_id = -1;
                continue;
            }

            if (strcmp(st->current_device, active_items[active_pos].device_path) != 0) {
                if (st->fd >= 0) {
                    close(st->fd);
                    st->fd = -1;
                }

                st->current_device[0] = '\0';
                st->acc_len = 0;
                st->has_latest = 0;
                st->last_sent = 0;
                st->plant_id = active_items[active_pos].plant_id;

                st->fd = serial_open_nonblock(active_items[active_pos].device_path);
                if (st->fd >= 0) {
                    strncpy(st->current_device,
                            active_items[active_pos].device_path,
                            sizeof(st->current_device) - 1);
                    st->current_device[sizeof(st->current_device) - 1] = '\0';
                    printf("sensing thread reading %s\n", st->current_device);
                }
            }

            if (st->plant_id != active_items[active_pos].plant_id) {
                st->plant_id = active_items[active_pos].plant_id;
                st->last_sent = 0;
            }

            if (st->fd < 0)
                continue;

            {
                int r = serial_readline_nonblock(
                    st->fd,
                    line,
                    sizeof(line),
                    st->acc,
                    &st->acc_len);

                if (r < 0) {
                    perror("[sensing] serial_readline_nonblock");
                    close(st->fd);
                    st->fd = -1;
                    st->current_device[0] = '\0';
                    st->acc_len = 0;
                    st->has_latest = 0;
                    st->last_sent = 0;
                    usleep(300000);
                    continue;
                }

                if (r > 0 && parse_line(line, &current)) {
                    st->latest = current;
                    st->has_latest = 1;
                }
            }

            if (st->has_latest) {
                time_t now = time(NULL);

                if (st->last_sent == 0 || now - st->last_sent >= SENSING_INTERVAL_SEC) {
                    printf("[sensing] send plant=%d temp=%.2f humi=%.2f soil=%d light=%d\n",
                           st->plant_id,
                           st->latest.temp,
                           st->latest.humi,
                           st->latest.soil,
                           st->latest.light);

                    if (send_to_sensor_thread(st->plant_id, &st->latest)) {
                        st->last_sent = now;
                    }
                }
            }
        }

        usleep(100000);
    }

    return NULL;
}
