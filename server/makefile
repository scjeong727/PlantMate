CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lmysqlclient

SERVER_SRC = \
server_main.c db.c db_queue.c db_thread.c \
user_repository.c plant_repository.c sensor_repository.c event_repository.c \
user_service.c plant_service.c sensor_service.c event_service.c \
sensor_thread.c request_thread.c command_queue.c watering_thread.c \
sensor_buffer.c event_log.c cache_preload.c sensing.c device_lock.c\
session_manager.c watering_lock.c plant_owner_cache.c plant_threshold_cache.c \
mqtt_adapter.c sensor_event_dispatcher.c mqtt_device_registry.c mqtt_topic_helper.c \
ros2_bridge.c

LDFLAGS = -lmysqlclient -lpthread


CLIENT_SRC = fake_sensor_client.c

SERVER_BIN = server
CLIENT_BIN = fake_sensor_client

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

run: $(SERVER_BIN)
	./$(SERVER_BIN)

sensor: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
