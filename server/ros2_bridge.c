#include "ros2_bridge.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server_config.h"

static int is_safe_action(const char* action)
{
    size_t i;

    if (!action || action[0] == '\0')
        return 0;

    for (i = 0; action[i] != '\0'; i++) {
        if (!isalnum((unsigned char)action[i]) &&
            action[i] != '_' &&
            action[i] != '-')
            return 0;
    }

    return 1;
}

static void copy_json_string(char* out, size_t out_size, const char* in)
{
    size_t pos = 0;
    size_t i;

    if (!out || out_size == 0)
        return;

    out[0] = '\0';
    if (!in)
        return;

    for (i = 0; in[i] != '\0' && pos + 2 < out_size; i++) {
        if (in[i] == '"' || in[i] == '\\') {
            if (pos + 3 >= out_size)
                break;
            out[pos++] = '\\';
            out[pos++] = in[i];
        } else if (in[i] == '\'' || (unsigned char)in[i] < 32) {
            out[pos++] = ' ';
        } else {
            out[pos++] = in[i];
        }
    }

    out[pos] = '\0';
}

int ros2_bridge_publish_command(int plant_id, const char* action, const char* detail)
{
    const char* topic;
    char escaped_detail[ROS2_BRIDGE_DETAIL_MAX * 2];
    char payload[512];
    char yaml[640];
    pid_t pid;
    int status;

    if (plant_id <= 0 || !is_safe_action(action)) {
        fprintf(stderr, "ros2 bridge: invalid command plant_id=%d action=%s\n",
                plant_id, action ? action : "(null)");
        return 0;
    }

    topic = getenv("PLANTMATE_ROS2_TOPIC");
    if (!topic || topic[0] == '\0')
        topic = server_config_get()->ros2_bridge_topic;
    if (!topic || topic[0] == '\0')
        topic = ROS2_BRIDGE_TOPIC_DEFAULT;

    copy_json_string(escaped_detail, sizeof(escaped_detail), detail ? detail : "");
    snprintf(payload, sizeof(payload),
             "{\"plantId\":%d,\"action\":\"%s\",\"detail\":\"%s\"}",
             plant_id, action, escaped_detail);
    snprintf(yaml, sizeof(yaml), "data: '%s'", payload);

    pid = fork();
    if (pid < 0) {
        perror("ros2 bridge fork");
        return 0;
    }

    if (pid == 0) {
        execlp("ros2", "ros2", "topic", "pub", "--once",
               topic, "std_msgs/msg/String", yaml, (char*)NULL);
        perror("ros2 bridge execlp");
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("ros2 bridge waitpid");
        return 0;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "ros2 bridge: publish failed status=%d\n", status);
        return 0;
    }

    printf("ros2 bridge published: topic=%s payload=%s\n", topic, payload);
    return 1;
}
