#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000

int main(void)
{
    int sock;
    struct sockaddr_in serv_addr;
    char req[256];
    char resp[1024];

    srand((unsigned int)time(NULL));

    int plant_id = 1;
    float temp = 20.0f + (rand() % 100) / 10.0f;
    float humi = 40.0f + (rand() % 300) / 10.0f;
    int soil = 300 + rand() % 300;
    int light = 500 + rand() % 300;

    snprintf(req, sizeof(req), "POST_SENSOR_DATA %d %.1f %.1f %d %d\n",
             plant_id, temp, humi, soil, light);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    send(sock, req, strlen(req), 0);

    int n = recv(sock, resp, sizeof(resp) - 1, 0);
    if (n > 0) {
        resp[n] = 0;
        printf("%s", resp);
    }

    close(sock);
    return 0;
}
