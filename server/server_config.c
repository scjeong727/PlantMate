#include "server_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ServerConfig g_config = {
    "127.0.0.1",
    "root",
    "1234",
    "plant_db",
    0,
    9000,
    1883,
    9001,
    "127.0.0.1",
    "/plantmate/robot_command"
};

static char* trim(char* text)
{
    char* end;

    while (*text && isspace((unsigned char)*text))
        text++;

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1)))
        end--;
    *end = '\0';

    return text;
}

static void copy_value(char* out, size_t out_size, const char* value)
{
    if (!out || out_size == 0 || !value)
        return;

    snprintf(out, out_size, "%s", value);
}

static void apply_pair(const char* key, const char* value)
{
    if (strcmp(key, "db_host") == 0)
        copy_value(g_config.db_host, sizeof(g_config.db_host), value);
    else if (strcmp(key, "db_user") == 0)
        copy_value(g_config.db_user, sizeof(g_config.db_user), value);
    else if (strcmp(key, "db_password") == 0)
        copy_value(g_config.db_password, sizeof(g_config.db_password), value);
    else if (strcmp(key, "db_name") == 0)
        copy_value(g_config.db_name, sizeof(g_config.db_name), value);
    else if (strcmp(key, "db_port") == 0)
        g_config.db_port = (unsigned int)atoi(value);
    else if (strcmp(key, "request_port") == 0)
        g_config.request_port = atoi(value);
    else if (strcmp(key, "mqtt_port") == 0)
        g_config.mqtt_port = atoi(value);
    else if (strcmp(key, "sensor_port") == 0)
        g_config.sensor_port = atoi(value);
    else if (strcmp(key, "sensing_server_ip") == 0)
        copy_value(g_config.sensing_server_ip, sizeof(g_config.sensing_server_ip), value);
    else if (strcmp(key, "ros2_bridge_topic") == 0)
        copy_value(g_config.ros2_bridge_topic, sizeof(g_config.ros2_bridge_topic), value);
}

void server_config_load(void)
{
    const char* path = getenv("PLANTMATE_SERVER_CONFIG");
    FILE* file;
    char line[256];

    if (!path || path[0] == '\0')
        path = "server_config.conf";

    file = fopen(path, "r");
    if (!file && strcmp(path, "server_config.conf") == 0) {
        path = "server/server_config.conf";
        file = fopen(path, "r");
    }

    if (!file) {
        printf("server config: using defaults, config file not found: %s\n", path);
        return;
    }

    while (fgets(line, sizeof(line), file)) {
        char* key;
        char* value;
        char* eq;

        key = trim(line);
        if (key[0] == '\0' || key[0] == '#')
            continue;

        eq = strchr(key, '=');
        if (!eq)
            continue;

        *eq = '\0';
        value = trim(eq + 1);
        key = trim(key);
        apply_pair(key, value);
    }

    fclose(file);
    printf("server config loaded: %s\n", path);
}

const ServerConfig* server_config_get(void)
{
    return &g_config;
}
