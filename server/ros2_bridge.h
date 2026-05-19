#ifndef ROS2_BRIDGE_H
#define ROS2_BRIDGE_H

#define ROS2_BRIDGE_TOPIC_DEFAULT "/plantmate/robot_command"
#define ROS2_BRIDGE_ACTION_MAX 64
#define ROS2_BRIDGE_DETAIL_MAX 256

int ros2_bridge_publish_command(int plant_id, const char* action, const char* detail);

#endif
