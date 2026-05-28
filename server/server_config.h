#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

typedef struct {
    char db_host[64];
    char db_user[64];
    char db_password[64];
    char db_name[64];
    unsigned int db_port;
    int request_port;
    int mqtt_port;
    int sensor_port;
    char sensing_server_ip[64];
    char ros2_bridge_topic[128];
} ServerConfig;

void server_config_load(void);
const ServerConfig* server_config_get(void);

#endif
