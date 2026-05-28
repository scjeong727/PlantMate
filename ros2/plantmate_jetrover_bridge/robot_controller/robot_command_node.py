#!/usr/bin/env python3
import json
import subprocess
import sys
import threading
from pathlib import Path
from typing import Callable, Dict, Tuple

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


def parse_detail(detail: str) -> Dict[str, str]:
    parsed = {}
    if not detail:
        return parsed

    try:
        loaded = json.loads(detail)
        if isinstance(loaded, dict):
            return loaded
    except Exception:
        pass

    for part in detail.split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        parsed[key.strip()] = value.strip()
    return parsed


class CommandNode(Node):
    SUPPORTED_ACTIONS = {"water", "move"}

    def __init__(self):
        super().__init__("command_node")

        self.declare_parameter("mqtt_host", "192.168.0.6")
        self.declare_parameter("mqtt_port", 1883)
        self.declare_parameter("device_type", "arm")
        self.declare_parameter("device_id", "robot-1")
        self.declare_parameter("command_topic", "/plantmate/robot_command")
        self.declare_parameter("ros_command_topic", "/plantmate/robot_command")

        self.mqtt_host = self.get_parameter("mqtt_host").get_parameter_value().string_value
        self.mqtt_port = self.get_parameter("mqtt_port").get_parameter_value().integer_value
        self.device_type = self.get_parameter("device_type").get_parameter_value().string_value
        self.device_id = self.get_parameter("device_id").get_parameter_value().string_value
        self.command_topic = self.get_parameter("command_topic").get_parameter_value().string_value
        self.ros_command_topic = self.get_parameter("ros_command_topic").get_parameter_value().string_value

        self.status_topic = f"device/{self.device_type}/{self.device_id}/status"
        self.script_dir = Path(__file__).resolve().parent
        self.water_lock = threading.Lock()
        self.move_lock = threading.Lock()
        self.water_running = False
        self.move_running = False

        self.create_subscription(String, self.ros_command_topic, self.on_ros_command, 10)

        try:
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
            self.mqtt_client.on_message = self.on_mqtt_message
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            self.mqtt_client.subscribe(self.command_topic)
            self.get_logger().info(
                f"MQTT connected! Subscribed to {self.command_topic} | Pub: {self.status_topic}"
            )
        except Exception as e:
            self.mqtt_client = None
            self.get_logger().error(f"Failed to connect MQTT: {e}")

    def publish_status(self, event_type: str, plant_id: int, detail: str = ""):
        if self.mqtt_client is None:
            return
        payload = {
            "eventType": event_type,
            "plantId": plant_id,
            "detail": detail,
        }
        self.mqtt_client.publish(self.status_topic, json.dumps(payload), qos=1)

    def publish_pong(self, data: dict):
        if self.mqtt_client is None:
            self.get_logger().warn("MQTT disconnected, PONG skipped")
            return
        payload = {
            "eventType": "PONG",
            "deviceType": self.device_type,
            "deviceId": self.device_id,
            "requestId": data.get("requestId", ""),
        }
        result = self.mqtt_client.publish(self.status_topic, json.dumps(payload), qos=1)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            self.get_logger().info(f"PONG sent: {payload}")
        else:
            self.get_logger().warn(f"PONG publish failed: rc={result.rc}")

    def is_targeted_to_this_device(self, data: dict):
        target_type = str(data.get("targetDeviceType", "")).strip()
        target_id = str(data.get("targetDeviceId", "")).strip()

        if target_type and target_type != self.device_type:
            return False
        if target_id and target_id != self.device_id:
            return False
        return True

    def parse_xy(self, detail_data: Dict[str, str], x_key: str = "x", y_key: str = "y") -> Tuple[float, float]:
        return float(detail_data.get(x_key, 0.0)), float(detail_data.get(y_key, 0.0))

    def parse_water_targets(self, detail: str) -> Tuple[float, float, float, float]:
        detail_data = parse_detail(detail)
        target_x, target_y = self.parse_xy(detail_data, "x", "y")
        home_x = float(
            detail_data.get(
                "home_x",
                detail_data.get("origin_x", detail_data.get("back_x", 0.0)),
            )
        )
        home_y = float(
            detail_data.get(
                "home_y",
                detail_data.get("origin_y", detail_data.get("back_y", 0.0)),
            )
        )
        return target_x, target_y, home_x, home_y

    def run_script(self, script_name: str, script_args=None):
        script_path = self.script_dir / script_name
        if not script_path.exists():
            raise FileNotFoundError(f"스크립트를 찾을 수 없습니다: {script_path}")
        self.get_logger().info(f"[script] start: {script_name}")
        timeout_sec = 90
        if script_name == "run_watering_demo.py":
            timeout_sec = 30
        elif script_name == "run_move_demo.py":
            timeout_sec = 150
        cmd = [sys.executable, str(script_path)]
        if script_args:
            cmd.extend(script_args)
        subprocess.run(
            cmd,
            cwd=str(self.script_dir),
            check=True,
            timeout=timeout_sec,
        )
        self.get_logger().info(f"[script] done: {script_name}")

    def start_sequence(
        self,
        lock: threading.Lock,
        running_attr: str,
        busy_message: str,
        worker_fn: Callable[[], None],
    ):
        with lock:
            if getattr(self, running_attr):
                self.get_logger().warn(busy_message)
                return
            setattr(self, running_attr, True)

        def runner():
            try:
                worker_fn()
            finally:
                with lock:
                    setattr(self, running_attr, False)

        threading.Thread(target=runner, daemon=True).start()

    def start_water_scripts(
        self,
        plant_id: int,
        target_x: float,
        target_y: float,
        home_x: float,
        home_y: float,
    ):
        def worker():
            try:
                detail = f"to=({target_x},{target_y}) home=({home_x},{home_y})"
                self.publish_status("WATER_SEQUENCE_STARTED", plant_id, detail)
                self.run_script("pick_demo.py")
                self.run_script(
                    "run_move_demo.py",
                    ["--x", str(target_x), "--y", str(target_y)],
                )
                self.run_script("run_watering_demo.py")
                self.run_script(
                    "run_move_demo.py",
                    ["--x", str(home_x), "--y", str(home_y)],
                )
                self.run_script("run_watering_end_demo.py")
                self.publish_status("WATER_SEQUENCE_DONE", plant_id)
            except Exception as e:
                self.get_logger().error(f"water sequence failed: {e}")
                self.publish_status("WATER_SEQUENCE_FAILED", plant_id, str(e))

        self.start_sequence(
            self.water_lock,
            "water_running",
            "water sequence already running",
            worker,
        )

    def start_move_script(self, plant_id: int, target_x: float, target_y: float):
        def worker():
            try:
                detail = f"x={target_x} y={target_y}"
                self.publish_status("MOVE_STARTED", plant_id, detail)
                self.run_script(
                    "run_move_demo.py",
                    ["--x", str(target_x), "--y", str(target_y)],
                )
                self.publish_status("MOVE_DONE", plant_id, detail)
            except Exception as e:
                self.get_logger().error(f"move sequence failed: {e}")
                self.publish_status("MOVE_FAILED", plant_id, str(e))

        self.start_sequence(
            self.move_lock,
            "move_running",
            "move sequence already running",
            worker,
        )

    def on_mqtt_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode("utf-8"))
            self.dispatch_command(data)
        except Exception as e:
            self.get_logger().error(f"Failed to process MQTT message: {e}")

    def on_ros_command(self, msg: String):
        try:
            data = json.loads(msg.data)
            self.dispatch_command(data)
        except Exception as e:
            self.get_logger().error(f"Invalid command json: {e}")

    def dispatch_command(self, data: dict):
        action_raw = str(data.get("action", "")).strip()
        action = action_raw.lower()
        try:
            plant_id = int(data.get("plantId", 0))
        except Exception:
            plant_id = 0
            self.get_logger().warn(f"Invalid plantId: {data.get('plantId')}, defaulting to 0")
        detail = str(data.get("detail", "")).strip()

        self.get_logger().info(
            f"Received command: plant_id={plant_id}, action={action_raw}, detail={detail}"
        )

        if not self.is_targeted_to_this_device(data):
            self.get_logger().info(
                f"Command ignored for target={data.get('targetDeviceType', '')}/{data.get('targetDeviceId', '')}"
            )
            return

        if action == "ping":
            self.publish_pong(data)
            return

        if action not in self.SUPPORTED_ACTIONS:
            self.get_logger().warn(f"Unsupported action: {action_raw}")
            return

        self.publish_status("COMMAND_RECEIVED", plant_id, detail)

        if action == "water":
            try:
                target_x, target_y, home_x, home_y = self.parse_water_targets(detail)
            except Exception as e:
                self.get_logger().error(f"물주기 좌표 파싱 실패: {e}")
                self.publish_status("WATER_SEQUENCE_FAILED", plant_id, str(e))
                return
            self.start_water_scripts(plant_id, target_x, target_y, home_x, home_y)
            return

        if action == "move":
            try:
                target_x, target_y = self.parse_xy(parse_detail(detail), "x", "y")
            except Exception as e:
                self.get_logger().error(f"좌표 파싱 실패: {e}")
                self.publish_status("MOVE_FAILED", plant_id, str(e))
                return
            self.start_move_script(plant_id, target_x, target_y)
            return

    def destroy_node(self):
        if self.mqtt_client is not None:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        super().destroy_node()


def main():
    rclpy.init()
    node = None
    try:
        node = CommandNode()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node is not None:
            node.destroy_node()
        try:
            rclpy.try_shutdown()
        except AttributeError:
            if rclpy.ok():
                rclpy.shutdown()


if __name__ == "__main__":
    main()

