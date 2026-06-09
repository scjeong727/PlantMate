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
    "turn_angle_tolerance": 0.08,
    "turn_angular_gain": 0.45,
    "turn_max_angular_speed": 0.16,
    "turn_min_angular_speed": 0.06,
    "turn_overshoot_tolerance": 0.14,
    "turn_timeout_sec": 25.0,
    "turn_no_progress_timeout_sec": 3.0,
    "turn_stable_cycles": 3,
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


def normalize_angle(angle: float):
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def yaw_from_quaternion(q):
    siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny_cosp, cosy_cosp)


def clamp(value: float, min_value: float, max_value: float):
    return max(min_value, min(max_value, value))


class TurnRunner(Node):
    def __init__(self, yaw_delta: float):
        super().__init__("turn_runner")
        self.yaw_delta = yaw_delta
        self.start_yaw = None
        self.current_yaw = 0.0
        self.done = False
        self.success = False
        self.stable_count = 0
        self.best_progress = 0.0
        self.last_progress_at = time.monotonic()
        self.start_time = time.monotonic()

        self.declare_parameter("config_path", str(Path(__file__).with_name("robot_config.json")))
        config_path = Path(self.get_parameter("config_path").get_parameter_value().string_value)
        config = load_config(config_path)

        self.declare_parameter("odom_topic", str(config["odom_topic"]))
        self.declare_parameter("cmd_vel_topic", str(config["cmd_vel_topic"]))
        self.declare_parameter("angle_tolerance", float(config["turn_angle_tolerance"]))
        self.declare_parameter("angular_gain", float(config["turn_angular_gain"]))
        self.declare_parameter("max_angular_speed", float(config["turn_max_angular_speed"]))
        self.declare_parameter("min_angular_speed", float(config["turn_min_angular_speed"]))
        self.declare_parameter("overshoot_tolerance", float(config["turn_overshoot_tolerance"]))
        self.declare_parameter("command_timeout_sec", float(config["turn_timeout_sec"]))
        self.declare_parameter("no_progress_timeout_sec", float(config["turn_no_progress_timeout_sec"]))
        self.declare_parameter("stable_cycles", int(config["turn_stable_cycles"]))

        self.odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        self.cmd_vel_topic = self.get_parameter("cmd_vel_topic").get_parameter_value().string_value
        self.angle_tolerance = self.get_parameter("angle_tolerance").get_parameter_value().double_value
        self.angular_gain = self.get_parameter("angular_gain").get_parameter_value().double_value
        self.max_angular_speed = self.get_parameter("max_angular_speed").get_parameter_value().double_value
        self.min_angular_speed = self.get_parameter("min_angular_speed").get_parameter_value().double_value
        self.overshoot_tolerance = self.get_parameter("overshoot_tolerance").get_parameter_value().double_value
        self.command_timeout_sec = self.get_parameter("command_timeout_sec").get_parameter_value().double_value
        self.no_progress_timeout_sec = self.get_parameter("no_progress_timeout_sec").get_parameter_value().double_value
        self.stable_cycles = self.get_parameter("stable_cycles").get_parameter_value().integer_value

        self.cmd_vel_pub = self.create_publisher(Twist, self.cmd_vel_topic, 10)
        self.create_subscription(Odometry, self.odom_topic, self.on_odom, 10)
        self.create_timer(0.1, self.control_loop)
        self.get_logger().info(f"[turn] start yaw_delta={self.yaw_delta:.3f}rad")

    def on_odom(self, msg: Odometry):
        self.current_yaw = yaw_from_quaternion(msg.pose.pose.orientation)
        if self.start_yaw is None:
            self.start_yaw = self.current_yaw

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
            self.finish(False, "[turn] timeout")
            return
        if self.start_yaw is None:
            return

        target_amount = abs(self.yaw_delta)
        if target_amount <= self.angle_tolerance:
            self.finish(True, "[turn] skipped small yaw")
            return

        direction = 1.0 if self.yaw_delta > 0.0 else -1.0
        turned = normalize_angle(self.current_yaw - self.start_yaw)
        progress = direction * turned
        remaining = target_amount - progress

        if progress > self.best_progress + 0.01:
            self.best_progress = progress
            self.last_progress_at = time.monotonic()
        elif time.monotonic() - self.last_progress_at > self.no_progress_timeout_sec:
            self.finish(
                False,
                f"[turn] no progress progress={progress:.3f} remaining={remaining:.3f}",
            )
            return

        if remaining <= self.angle_tolerance or (
            progress >= target_amount and abs(remaining) <= self.overshoot_tolerance
        ):
            self.stable_count += 1
            self.cmd_vel_pub.publish(Twist())
            if self.stable_count >= max(1, self.stable_cycles):
                self.finish(True, f"[turn] done remaining={remaining:.3f}")
            return

        self.stable_count = 0
        cmd = Twist()
        speed = clamp(
            self.angular_gain * max(0.0, remaining),
            self.min_angular_speed,
            self.max_angular_speed,
        )
        cmd.angular.z = direction * speed
        self.cmd_vel_pub.publish(cmd)

    def destroy_node(self):
        self.publish_stop()
        super().destroy_node()


def parse_args():
    parser = argparse.ArgumentParser(description="Relative turn script")
    parser.add_argument("--yaw", type=float, required=True, help="Relative yaw in radians")
    return parser.parse_args()


def main():
    args = parse_args()
    rclpy.init()
    node = None
    exit_code = 1
    try:
        node = TurnRunner(args.yaw)
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
