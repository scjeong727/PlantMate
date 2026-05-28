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

#from move_controller import MoveController


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

        self.status_topic = f"device/{self.device_type}/{self.device_id}/status"
        self.script_dir = Path(__file__).resolve().parent
        self.water_running = False
        self.water_lock = threading.Lock()
        self.valid_actions = {"water", "move"}

        #self.move_controller = MoveController(self)

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

    def run_script(self, script_name: str):
        script_path = self.script_dir / script_name
        if not script_path.exists():
            raise FileNotFoundError(f"스크립트를 찾을 수 없습니다: {script_path}")
        self.get_logger().info(f"[script] start: {script_name}")
        timeout_sec = 90
        if script_name == "run_watering_demo.py":
            timeout_sec = 30
        subprocess.run(
            [sys.executable, str(script_path)],
            cwd=str(self.script_dir),
            check=True,
            timeout=timeout_sec,
        )
        self.get_logger().info(f"[script] done: {script_name}")

    def start_water_scripts(self, plant_id: int):
        with self.water_lock:
            if self.water_running:
                self.get_logger().warn("water sequence already running")
                return
            self.water_running = True

        def worker():
            try:
                self.publish_status("WATER_SEQUENCE_STARTED", plant_id)
                self.run_script("pick_demo.py")
                self.run_script("run_watering_demo.py")
                self.run_script("run_watering_end_demo.py")
                self.publish_status("WATER_SEQUENCE_DONE", plant_id)
            except Exception as e:
                self.get_logger().error(f"water sequence failed: {e}")
                self.publish_status("WATER_SEQUENCE_FAILED", plant_id, str(e))
            finally:
                with self.water_lock:
                    self.water_running = False

        threading.Thread(target=worker, daemon=True).start()

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
        plant_id = int(data.get("plantId", 0))
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

        self.publish_status("COMMAND_RECEIVED", plant_id, detail)

        if action == "water":
            self.start_water_scripts(plant_id)
            return

        if action == "move":
            self.get_logger().warn("move action is currently disabled in simplified command_node")
            return

        if action not in self.valid_actions:
            self.get_logger().warn(f"Unsupported action: {action_raw}")
            return

        # if action == "move":
        #     detail_data = parse_detail(detail)
        #     try:
        #         target_x = float(detail_data.get("x", 0.0))
        #         target_y = float(detail_data.get("y", 0.0))
        #     except Exception as e:
        #         self.get_logger().error(f"좌표 파싱 실패: {e}")
        #         return
        #
        #     def move_worker():
        #         try:
        #             self.move_controller.move_to_and_wait(target_x, target_y)
        #             self.publish_status("MOVE_DONE", plant_id, f"x={target_x} y={target_y}")
        #         except Exception as ex:
        #             self.get_logger().error(f"move failed: {ex}")
        #             self.publish_status("MOVE_FAILED", plant_id, str(ex))
        #
        #     threading.Thread(target=move_worker, daemon=True).start()
        #     return

    def destroy_node(self):
        if self.mqtt_client is not None:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        super().destroy_node()


def main():
    rclpy.init()
    node = CommandNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

