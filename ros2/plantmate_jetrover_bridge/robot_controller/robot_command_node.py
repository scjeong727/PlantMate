#!/usr/bin/env python3
import json
import subprocess
import sys
import threading
from pathlib import Path

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


def parse_detail(detail: str):
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
        self.device_command_topics = [
            f"device/{self.device_type}/{self.device_id}/move/command",
            f"device/{self.device_type}/{self.device_id}/water/command",
        ]

        self.status_topic = f"device/{self.device_type}/{self.device_id}/status"
        self.script_dir = Path(__file__).resolve().parent
        self.task_lock = threading.Lock()
        self.active_task = None

        self.create_subscription(String, self.ros_command_topic, self.on_ros_command, 10)

        try:
            client_id = f"command-{self.device_type}-{self.device_id}"
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=client_id)
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
            self.mqtt_client.on_message = self.on_mqtt_message
            self.mqtt_client.reconnect_delay_set(min_delay=1, max_delay=10)
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            self.get_logger().info(
                f"MQTT connecting to {self.mqtt_host}:{self.mqtt_port} | Pub: {self.status_topic}"
            )
        except Exception as e:
            self.mqtt_client = None
            self.get_logger().error(f"Failed to connect MQTT: {e}")

    def mqtt_subscribe_topics(self, client):
        subscribe_topics = list(dict.fromkeys([self.command_topic] + self.device_command_topics))
        for topic in subscribe_topics:
            client.subscribe(topic, qos=1)
        self.get_logger().info(
            f"MQTT connected! Subscribed to {', '.join(subscribe_topics)} | Pub: {self.status_topic}"
        )

    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc != 0:
            self.get_logger().error(f"MQTT connect failed: rc={rc}")
            return
        self.mqtt_subscribe_topics(client)

    def on_mqtt_disconnect(self, client, userdata, rc):
        if rc == 0:
            self.get_logger().info("MQTT disconnected")
        else:
            self.get_logger().warn(f"MQTT disconnected unexpectedly: rc={rc}; reconnecting")

    def publish_status(self, event_type: str, plant_id: int, detail: str = ""):
        if self.mqtt_client is None:
            return
        payload = {
            "eventType": event_type,
            "plantId": plant_id,
            "detail": detail,
        }
        self.mqtt_client.publish(self.status_topic, json.dumps(payload), qos=1)

    def is_targeted_to_this_device(self, data: dict):
        target_type = str(data.get("targetDeviceType", "")).strip()
        target_id = str(data.get("targetDeviceId", "")).strip()
        if target_type and target_type != self.device_type:
            return False
        if target_id and target_id != self.device_id:
            return False
        return True

    def acquire_task(self, task_name: str):
        with self.task_lock:
            if self.active_task is not None:
                return False, self.active_task
            self.active_task = task_name
            return True, ""

    def release_task(self):
        with self.task_lock:
            self.active_task = None

    def run_script(self, script_name: str, args=None, timeout_sec: int = 120):
        script_path = self.script_dir / script_name
        if not script_path.exists():
            raise FileNotFoundError(f"missing script: {script_path}")

        cmd = [sys.executable, str(script_path)]
        if args:
            cmd.extend(args)

        self.get_logger().info(f"[script] start: {' '.join(cmd)}")
        subprocess.run(cmd, cwd=str(self.script_dir), check=True, timeout=timeout_sec)
        self.get_logger().info(f"[script] done: {script_name}")

    def start_task(self, task_name: str, plant_id: int, detail: str, worker_fn):
        ok, running = self.acquire_task(task_name)
        if not ok:
            self.publish_status(f"{task_name.upper()}_FAILED", plant_id, f"task busy: {running}")
            self.get_logger().warn(f"task busy: {running}")
            return

        def worker():
            try:
                self.publish_status(f"{task_name.upper()}_STARTED", plant_id, detail)
                worker_fn()
                self.publish_status(f"{task_name.upper()}_DONE", plant_id, detail)
            except Exception as e:
                self.get_logger().error(f"{task_name} failed: {e}")
                self.publish_status(f"{task_name.upper()}_FAILED", plant_id, str(e))
            finally:
                self.release_task()

        threading.Thread(target=worker, daemon=True).start()

    def start_move(self, plant_id: int, target_x: float, target_y: float):
        detail = f"x={target_x} y={target_y}"

        def worker():
            self.run_script(
                "run_move_demo.py",
                ["--x", str(target_x), "--y", str(target_y)],
                timeout_sec=150,
            )

        self.start_task("move", plant_id, detail, worker)

    def start_water(self, plant_id: int, target_x: float, target_y: float, home_x=None, home_y=None):
        detail = f"x={target_x} y={target_y}"
        script_args = ["--x", str(target_x), "--y", str(target_y)]
        if home_x is not None and home_y is not None:
            detail += f" home_x={home_x} home_y={home_y}"
            script_args.extend(["--home-x", str(home_x), "--home-y", str(home_y)])

        def worker():
            self.run_script(
                "run_water_sequence.py",
                script_args,
                timeout_sec=240,
            )

        self.start_task("water_sequence", plant_id, detail, worker)

    def dispatch_command(self, data: dict):
        action_raw = str(data.get("action", "")).strip()
        action = action_raw.lower()
        try:
            plant_id = int(data.get("plantId", 0))
        except Exception:
            plant_id = 0
        detail = str(data.get("detail", "")).strip()

        self.get_logger().info(
            f"Received command: plant_id={plant_id}, action={action_raw}, detail={detail}"
        )

        if not self.is_targeted_to_this_device(data):
            return

        if action == "ping":
            self.get_logger().debug("PING ignored by command node; handled by robot_heartbeat_node")
            return

        if action not in {"move", "water"}:
            self.get_logger().warn(f"Unsupported action: {action_raw}")
            return

        self.publish_status("COMMAND_RECEIVED", plant_id, detail)
        detail_data = parse_detail(detail)

        if action == "move":
            try:
                target_x = float(detail_data.get("x", 0.0))
                target_y = float(detail_data.get("y", 0.0))
            except Exception as e:
                self.publish_status("MOVE_FAILED", plant_id, f"invalid move coordinate: {e}")
                self.get_logger().error(f"invalid move coordinate: {e}")
                return
            self.start_move(plant_id, target_x, target_y)
            return

        try:
            target_x = float(detail_data.get("x", 0.0))
            target_y = float(detail_data.get("y", 0.0))
            has_home_x = any(key in detail_data for key in ("home_x", "origin_x", "back_x"))
            has_home_y = any(key in detail_data for key in ("home_y", "origin_y", "back_y"))
            home_x = None
            home_y = None
            if has_home_x or has_home_y:
                home_x = float(
                    detail_data.get("home_x", detail_data.get("origin_x", detail_data.get("back_x", 0.0)))
                )
                home_y = float(
                    detail_data.get("home_y", detail_data.get("origin_y", detail_data.get("back_y", 0.0)))
                )
        except Exception as e:
            self.publish_status("WATER_SEQUENCE_FAILED", plant_id, f"invalid water coordinate: {e}")
            self.get_logger().error(f"invalid water coordinate: {e}")
            return
        self.start_water(plant_id, target_x, target_y, home_x, home_y)

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
