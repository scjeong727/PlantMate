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
    "goal_tolerance": 0.15,
    "angle_tolerance": 0.2,
    "linear_gain": 0.35,
    "angular_gain": 0.7,
    "max_linear_speed": 0.25,
    "max_angular_speed": 0.4,
    "command_timeout_sec": 120.0,
    "heading_deadband": 0.08,
    "slow_distance": 0.35,
    "allow_reverse": True,
    "final_goal_tolerance": 0.03,
    "final_yaw_tolerance": 0.04,
    "final_max_linear_speed": 0.06,
    "final_max_angular_speed": 0.2,
    "final_align_timeout_sec": 15.0,
    "stable_cycles": 10,
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


class MoveRunner(Node):
    def __init__(
        self,
        target_x: float,
        target_y: float,
        origin_state_path: Path = None,
        save_origin: bool = False,
        final_yaw_origin: bool = False,
    ):
        super().__init__("move_runner")

        self.target_x = target_x
        self.target_y = target_y
        self.origin_state_path = origin_state_path
        self.save_origin = save_origin
        self.final_yaw_origin = final_yaw_origin

        self.raw_x = 0.0
        self.raw_y = 0.0
        self.raw_yaw = 0.0
        self.current_x = 0.0
        self.current_y = 0.0
        self.current_yaw = 0.0
        self.origin_x = 0.0
        self.origin_y = 0.0
        self.origin_yaw = 0.0
        self.has_odom = False
        self.has_origin = False
        self.position_reached = False
        self.stable_count = 0
        self.final_align_started_at = None
        self.drive_aligned = False
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

        self.declare_parameter("odom_topic", str(config["odom_topic"]))
        self.declare_parameter("cmd_vel_topic", str(config["cmd_vel_topic"]))
        self.declare_parameter("goal_tolerance", float(config["goal_tolerance"]))
        self.declare_parameter("angle_tolerance", float(config["angle_tolerance"]))
        self.declare_parameter("linear_gain", float(config["linear_gain"]))
        self.declare_parameter("angular_gain", float(config["angular_gain"]))
        self.declare_parameter("max_linear_speed", float(config["max_linear_speed"]))
        self.declare_parameter("max_angular_speed", float(config["max_angular_speed"]))
        self.declare_parameter("command_timeout_sec", float(config["command_timeout_sec"]))
        self.declare_parameter("heading_deadband", float(config["heading_deadband"]))
        self.declare_parameter("slow_distance", float(config["slow_distance"]))
        self.declare_parameter("allow_reverse", bool(config["allow_reverse"]))
        self.declare_parameter("final_goal_tolerance", float(config["final_goal_tolerance"]))
        self.declare_parameter("final_yaw_tolerance", float(config["final_yaw_tolerance"]))
        self.declare_parameter("final_max_linear_speed", float(config["final_max_linear_speed"]))
        self.declare_parameter("final_max_angular_speed", float(config["final_max_angular_speed"]))
        self.declare_parameter("final_align_timeout_sec", float(config["final_align_timeout_sec"]))
        self.declare_parameter("stable_cycles", int(config["stable_cycles"]))

        self.odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        self.cmd_vel_topic = self.get_parameter("cmd_vel_topic").get_parameter_value().string_value
        self.goal_tolerance = self.get_parameter("goal_tolerance").get_parameter_value().double_value
        self.angle_tolerance = self.get_parameter("angle_tolerance").get_parameter_value().double_value
        self.linear_gain = self.get_parameter("linear_gain").get_parameter_value().double_value
        self.angular_gain = self.get_parameter("angular_gain").get_parameter_value().double_value
        self.max_linear_speed = self.get_parameter("max_linear_speed").get_parameter_value().double_value
        self.max_angular_speed = self.get_parameter("max_angular_speed").get_parameter_value().double_value
        self.command_timeout_sec = self.get_parameter("command_timeout_sec").get_parameter_value().double_value
        self.heading_deadband = self.get_parameter("heading_deadband").get_parameter_value().double_value
        self.slow_distance = self.get_parameter("slow_distance").get_parameter_value().double_value
        self.allow_reverse = self.get_parameter("allow_reverse").get_parameter_value().bool_value
        self.final_goal_tolerance = self.get_parameter("final_goal_tolerance").get_parameter_value().double_value
        self.final_yaw_tolerance = self.get_parameter("final_yaw_tolerance").get_parameter_value().double_value
        self.final_max_linear_speed = self.get_parameter("final_max_linear_speed").get_parameter_value().double_value
        self.final_max_angular_speed = self.get_parameter("final_max_angular_speed").get_parameter_value().double_value
        self.final_align_timeout_sec = self.get_parameter("final_align_timeout_sec").get_parameter_value().double_value
        self.stable_cycles = self.get_parameter("stable_cycles").get_parameter_value().integer_value

        self.cmd_vel_pub = self.create_publisher(Twist, self.cmd_vel_topic, 10)
        self.create_subscription(Odometry, self.odom_topic, self.on_odom, 10)
        self.create_timer(0.1, self.control_loop)
        self.load_origin_state()

        self.get_logger().info(f"[move] start relative goal -> x={self.target_x}, y={self.target_y}")

    def load_origin_state(self):
        if self.origin_state_path is None or not self.origin_state_path.exists():
            return

        with self.origin_state_path.open("r", encoding="utf-8") as file:
            state = json.load(file)

        self.origin_x = float(state["origin_x"])
        self.origin_y = float(state["origin_y"])
        self.origin_yaw = float(state["origin_yaw"])
        self.has_origin = True
        self.get_logger().info(
            f"[move] loaded origin state: x={self.origin_x:.3f}, y={self.origin_y:.3f}, yaw={self.origin_yaw:.3f}"
        )

    def save_origin_state(self):
        if self.origin_state_path is None or not self.save_origin:
            return

        state = {
            "origin_x": self.origin_x,
            "origin_y": self.origin_y,
            "origin_yaw": self.origin_yaw,
        }
        self.origin_state_path.parent.mkdir(parents=True, exist_ok=True)
        with self.origin_state_path.open("w", encoding="utf-8") as file:
            json.dump(state, file)
        self.get_logger().info(f"[move] saved origin state: {self.origin_state_path}")

    def on_odom(self, msg: Odometry):
        pose = msg.pose.pose
        self.raw_x = pose.position.x
        self.raw_y = pose.position.y
        self.raw_yaw = yaw_from_quaternion(pose.orientation)
        self.has_odom = True

        if not self.has_origin:
            self.origin_x = self.raw_x
            self.origin_y = self.raw_y
            self.origin_yaw = self.raw_yaw
            self.has_origin = True
            self.save_origin_state()

        dx = self.raw_x - self.origin_x
        dy = self.raw_y - self.origin_y
        cos_yaw = math.cos(-self.origin_yaw)
        sin_yaw = math.sin(-self.origin_yaw)
        self.current_x = dx * cos_yaw - dy * sin_yaw
        self.current_y = dx * sin_yaw + dy * cos_yaw
        self.current_yaw = normalize_angle(self.raw_yaw - self.origin_yaw)

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

        elapsed = time.monotonic() - self.start_time
        if elapsed > self.command_timeout_sec:
            self.finish(False, f"[move] timeout after {elapsed:.1f}s")
            return

        if not self.has_odom:
            return

        dx = self.target_x - self.current_x
        dy = self.target_y - self.current_y
        distance = math.hypot(dx, dy)
        active_goal_tolerance = self.final_goal_tolerance if self.final_yaw_origin else self.goal_tolerance
        active_yaw_tolerance = self.final_yaw_tolerance if self.final_yaw_origin else self.angle_tolerance

        if distance <= active_goal_tolerance:
            self.position_reached = True
            if self.final_align_started_at is None:
                self.final_align_started_at = time.monotonic()

        if self.position_reached:
            if self.final_yaw_origin:
                yaw_error = normalize_angle(0.0 - self.current_yaw)
                align_elapsed = 0.0
                if self.final_align_started_at is not None:
                    align_elapsed = time.monotonic() - self.final_align_started_at
                if self.final_align_timeout_sec > 0.0 and align_elapsed >= self.final_align_timeout_sec:
                    self.finish(
                        True,
                        f"[move] final align timeout dist={distance:.3f}, yaw_error={yaw_error:.3f}",
                    )
                    return
                if distance > active_goal_tolerance:
                    self.position_reached = False
                    self.stable_count = 0
                    self.final_align_started_at = None
                    return

                if abs(yaw_error) > active_yaw_tolerance:
                    self.stable_count = 0
                    cmd = Twist()
                    cmd.angular.z = clamp(
                        self.angular_gain * yaw_error,
                        -self.final_max_angular_speed,
                        self.final_max_angular_speed,
                    )
                    self.cmd_vel_pub.publish(cmd)
                    return

                self.stable_count += 1
                if self.stable_count < max(1, self.stable_cycles):
                    self.cmd_vel_pub.publish(Twist())
                    return
                self.finish(True, f"[move] arrived dist={distance:.3f}, yaw_error={yaw_error:.3f}")
                return

            self.stable_count += 1
            if self.stable_count < max(1, self.stable_cycles):
                self.cmd_vel_pub.publish(Twist())
                return
            self.finish(True, f"[move] arrived dist={distance:.3f}")
            return

        target_yaw = math.atan2(dy, dx)
        yaw_error = normalize_angle(target_yaw - self.current_yaw)

        cmd = Twist()
        effective_max_linear = self.max_linear_speed
        effective_max_angular = self.max_angular_speed
        min_speed_scale = 0.35
        if self.final_yaw_origin:
            effective_max_linear = min(effective_max_linear, self.final_max_linear_speed)
            effective_max_angular = min(effective_max_angular, self.final_max_angular_speed)
            min_speed_scale = 0.2
        if self.slow_distance > 0.0 and distance < self.slow_distance:
            scale = max(min_speed_scale, distance / self.slow_distance)
            effective_max_linear *= scale
            effective_max_angular *= scale

        if not self.drive_aligned:
            if abs(yaw_error) > self.angle_tolerance:
                cmd.angular.z = clamp(
                    self.angular_gain * yaw_error,
                    -effective_max_angular,
                    effective_max_angular,
                )
                self.cmd_vel_pub.publish(cmd)
                return
            self.drive_aligned = True

        if abs(yaw_error) > self.heading_deadband:
            self.drive_aligned = False
            cmd.angular.z = clamp(
                self.angular_gain * yaw_error,
                -effective_max_angular,
                effective_max_angular,
            )
            self.cmd_vel_pub.publish(cmd)
            return

        linear = clamp(self.linear_gain * distance, 0.0, effective_max_linear)
        cmd.linear.x = linear
        self.cmd_vel_pub.publish(cmd)

    def destroy_node(self):
        self.publish_stop()
        super().destroy_node()


def parse_args():
    parser = argparse.ArgumentParser(description="Relative move script")
    parser.add_argument("--x", type=float, default=0.0, help="Relative target X")
    parser.add_argument("--y", type=float, default=0.0, help="Relative target Y")
    parser.add_argument("--origin-state", default="", help="Path to load/save origin pose state")
    parser.add_argument("--save-origin", action="store_true", help="Save first odom pose as origin state")
    parser.add_argument("--final-yaw-origin", action="store_true", help="Align final yaw to the saved origin yaw")
    return parser.parse_args()


def main():
    args = parse_args()
    rclpy.init()
    node = None
    exit_code = 1
    try:
        origin_state_path = Path(args.origin_state) if args.origin_state else None
        node = MoveRunner(args.x, args.y, origin_state_path, args.save_origin, args.final_yaw_origin)
        while rclpy.ok() and not node.done:
            rclpy.spin_once(node, timeout_sec=0.1)
        exit_code = 0 if (node and node.success) else 1
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
