#!/usr/bin/env python3
import argparse
import json
import math
import time
from pathlib import Path

import rclpy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Odometry
from rclpy.node import Node


DEFAULT_CONFIG = {
    "nav_goal_topic": "/goal_pose",
    "odom_topic": "/odom",
    "goal_tolerance": 0.15,
    "command_timeout_sec": 120.0,
    "goal_frame_id": "map",
}


def load_config(path: Path):
    config = dict(DEFAULT_CONFIG)
    if not path.exists():
        return config

    with path.open("r", encoding="utf-8") as file:
        loaded = json.load(file)

    if not isinstance(loaded, dict):
        raise ValueError("config root must be a JSON object")

    for key in config:
        if key in loaded:
            config[key] = loaded[key]
    return config


class MoveGoalRunner(Node):
    def __init__(self, target_x: float, target_y: float):
        super().__init__("move_goal_runner")

        self.target_x = target_x
        self.target_y = target_y
        self.current_x = 0.0
        self.current_y = 0.0
        self.has_odom = False
        self.done = False
        self.success = False
        self.start_time = time.monotonic()

        self.declare_parameter(
            "config_path",
            str(Path(__file__).with_name("robot_config.json")),
        )
        config_path = Path(self.get_parameter("config_path").get_parameter_value().string_value)
        try:
            config = load_config(config_path)
            self.get_logger().info(f"Loaded config: {config_path}")
        except Exception as e:
            config = dict(DEFAULT_CONFIG)
            self.get_logger().error(f"Failed to load config {config_path}: {e}. Using defaults.")

        self.declare_parameter("nav_goal_topic", str(config["nav_goal_topic"]))
        self.declare_parameter("odom_topic", str(config["odom_topic"]))
        self.declare_parameter("goal_tolerance", float(config["goal_tolerance"]))
        self.declare_parameter("command_timeout_sec", float(config["command_timeout_sec"]))
        self.declare_parameter("goal_frame_id", str(config["goal_frame_id"]))

        self.nav_goal_topic = self.get_parameter("nav_goal_topic").get_parameter_value().string_value
        self.odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        self.goal_tolerance = self.get_parameter("goal_tolerance").get_parameter_value().double_value
        self.command_timeout_sec = self.get_parameter("command_timeout_sec").get_parameter_value().double_value
        self.goal_frame_id = self.get_parameter("goal_frame_id").get_parameter_value().string_value

        self.nav_pub = self.create_publisher(PoseStamped, self.nav_goal_topic, 10)
        self.create_subscription(Odometry, self.odom_topic, self.on_odom, 10)
        self.create_timer(1.0, self.publish_goal)
        self.create_timer(0.1, self.watch_goal)

        self.publish_goal()
        self.get_logger().info(
            f"[move] goal start -> X: {self.target_x:.3f}, Y: {self.target_y:.3f}"
        )

    def on_odom(self, msg: Odometry):
        pose = msg.pose.pose
        self.current_x = pose.position.x
        self.current_y = pose.position.y
        self.has_odom = True

    def publish_goal(self):
        if self.done:
            return
        goal_msg = PoseStamped()
        goal_msg.header.frame_id = self.goal_frame_id
        goal_msg.header.stamp = self.get_clock().now().to_msg()
        goal_msg.pose.position.x = self.target_x
        goal_msg.pose.position.y = self.target_y
        goal_msg.pose.orientation.w = 1.0
        self.nav_pub.publish(goal_msg)

    def watch_goal(self):
        if self.done:
            return

        elapsed = time.monotonic() - self.start_time
        if elapsed > self.command_timeout_sec:
            self.done = True
            self.success = False
            self.get_logger().error(
                f"[move] timeout after {elapsed:.1f}s -> X: {self.target_x:.3f}, Y: {self.target_y:.3f}"
            )
            return

        if not self.has_odom:
            return

        distance = math.hypot(self.target_x - self.current_x, self.target_y - self.current_y)
        if distance <= self.goal_tolerance:
            self.done = True
            self.success = True
            self.get_logger().info(
                f"[move] arrived -> X: {self.target_x:.3f}, Y: {self.target_y:.3f}, dist={distance:.3f}"
            )


def parse_args():
    parser = argparse.ArgumentParser(description="Simple move goal script")
    parser.add_argument("--x", type=float, default=0.0, help="Target X")
    parser.add_argument("--y", type=float, default=0.0, help="Target Y")
    return parser.parse_args()


def main():
    args = parse_args()
    rclpy.init()
    node = None
    exit_code = 1
    try:
        node = MoveGoalRunner(args.x, args.y)
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
