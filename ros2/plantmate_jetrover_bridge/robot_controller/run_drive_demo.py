#!/usr/bin/env python3
import argparse
import json
import math
import time
from pathlib import Path

import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node


DEFAULT_CONFIG = {
    "odom_topic": "/odom",
    "cmd_vel_topic": "/controller/cmd_vel",
    "drive_goal_tolerance": 0.04,
    "drive_linear_gain": 0.35,
    "drive_max_linear_speed": 0.12,
    "drive_timeout_sec": 90.0,
    "drive_stable_cycles": 8,
}


def load_config(path: Path):
    config = dict(DEFAULT_CONFIG)
    if not path.exists():
        return config
    with path.open("r", encoding="utf-8") as file:
        loaded = json.load(file)
    if isinstance(loaded, dict):
        for key in config:
            if key in loaded:
                config[key] = loaded[key]
    return config


def yaw_from_quaternion(q):
    siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny_cosp, cosy_cosp)


def clamp(value: float, min_value: float, max_value: float):
    return max(min_value, min(max_value, value))


class DriveRunner(Node):
    def __init__(self, distance: float):
        super().__init__("drive_runner")
        self.target_distance = distance
        self.start_x = None
        self.start_y = None
        self.start_yaw = 0.0
        self.current_x = 0.0
        self.current_y = 0.0
        self.done = False
        self.success = False
        self.stable_count = 0
        self.start_time = time.monotonic()

        self.declare_parameter("config_path", str(Path(__file__).with_name("robot_config.json")))
        config_path = Path(self.get_parameter("config_path").get_parameter_value().string_value)
        config = load_config(config_path)

        self.declare_parameter("odom_topic", str(config["odom_topic"]))
        self.declare_parameter("cmd_vel_topic", str(config["cmd_vel_topic"]))
        self.declare_parameter("goal_tolerance", float(config["drive_goal_tolerance"]))
        self.declare_parameter("linear_gain", float(config["drive_linear_gain"]))
        self.declare_parameter("max_linear_speed", float(config["drive_max_linear_speed"]))
        self.declare_parameter("command_timeout_sec", float(config["drive_timeout_sec"]))
        self.declare_parameter("stable_cycles", int(config["drive_stable_cycles"]))

        self.odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        self.cmd_vel_topic = self.get_parameter("cmd_vel_topic").get_parameter_value().string_value
        self.goal_tolerance = self.get_parameter("goal_tolerance").get_parameter_value().double_value
        self.linear_gain = self.get_parameter("linear_gain").get_parameter_value().double_value
        self.max_linear_speed = self.get_parameter("max_linear_speed").get_parameter_value().double_value
        self.command_timeout_sec = self.get_parameter("command_timeout_sec").get_parameter_value().double_value
        self.stable_cycles = self.get_parameter("stable_cycles").get_parameter_value().integer_value

        self.cmd_vel_pub = self.create_publisher(Twist, self.cmd_vel_topic, 10)
        self.create_subscription(Odometry, self.odom_topic, self.on_odom, 10)
        self.create_timer(0.1, self.control_loop)
        self.get_logger().info(f"[drive] start distance={self.target_distance:.3f}m")

    def on_odom(self, msg: Odometry):
        pose = msg.pose.pose
        self.current_x = pose.position.x
        self.current_y = pose.position.y
        if self.start_x is None:
            self.start_x = self.current_x
            self.start_y = self.current_y
            self.start_yaw = yaw_from_quaternion(pose.orientation)

    def traveled_distance(self):
        if self.start_x is None:
            return 0.0
        dx = self.current_x - self.start_x
        dy = self.current_y - self.start_y
        return dx * math.cos(self.start_yaw) + dy * math.sin(self.start_yaw)

    def publish_stop(self):
        stop = Twist()
        for _ in range(5):
            self.cmd_vel_pub.publish(stop)
            time.sleep(0.02)

    def finish(self, success: bool, message: str):
        if self.done:
            return
        self.done = True
        self.success = success
        self.publish_stop()
        if success:
            self.get_logger().info(message)
        else:
            self.get_logger().error(message)

    def control_loop(self):
        if self.done:
            return
        if time.monotonic() - self.start_time > self.command_timeout_sec:
            self.finish(False, "[drive] timeout")
            return
        if self.start_x is None:
            return

        traveled = self.traveled_distance()
        error = self.target_distance - traveled
        if abs(error) <= self.goal_tolerance:
            self.stable_count += 1
            self.cmd_vel_pub.publish(Twist())
            if self.stable_count >= max(1, self.stable_cycles):
                self.finish(True, f"[drive] done error={error:.3f}")
            return

        self.stable_count = 0
        cmd = Twist()
        speed = clamp(abs(error) * self.linear_gain, 0.03, self.max_linear_speed)
        cmd.linear.x = speed if error > 0 else -speed
        self.cmd_vel_pub.publish(cmd)

    def destroy_node(self):
        self.publish_stop()
        super().destroy_node()


def parse_args():
    parser = argparse.ArgumentParser(description="Straight drive script")
    parser.add_argument("--distance", type=float, required=True, help="Signed distance in meters")
    return parser.parse_args()


def main():
    args = parse_args()
    rclpy.init()
    node = None
    exit_code = 1
    try:
        node = DriveRunner(args.distance)
        while rclpy.ok() and not node.done:
            rclpy.spin_once(node, timeout_sec=0.1)
        exit_code = 0 if node and node.success else 1
    except KeyboardInterrupt:
        exit_code = 130
    finally:
        if node is not None:
            node.destroy_node()
        try:
            rclpy.try_shutdown()
        except AttributeError:
            if rclpy.ok():
                rclpy.shutdown()
    raise SystemExit(exit_code)


if __name__ == "__main__":
    main()
