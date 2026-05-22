#!/usr/bin/env python3
import json
from pathlib import Path

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node


DEFAULT_CONFIG = {
    'mqtt_host': '192.168.0.6',
    'mqtt_port': 1883,
    'device_type': 'arm',
    'device_id': 'robot-1',
    'command_topic': '/plantmate/robot_command',
}


def load_config(path):
    config = dict(DEFAULT_CONFIG)
    if not path.exists():
        return config

    with path.open('r', encoding='utf-8') as file:
        loaded = json.load(file)

    if not isinstance(loaded, dict):
        raise ValueError('config root must be a JSON object')

    for key in config:
        if key in loaded:
            config[key] = loaded[key]
    return config


class RobotHeartbeatNode(Node):
    def __init__(self):
        super().__init__('robot_heartbeat_node')

        self.declare_parameter(
            'config_path',
            str(Path(__file__).with_name('robot_config.json')),
        )
        config_path = Path(self.get_parameter('config_path').get_parameter_value().string_value)
        try:
            config = load_config(config_path)
            self.get_logger().info(f'Loaded config: {config_path}')
        except Exception as exc:
            config = dict(DEFAULT_CONFIG)
            self.get_logger().error(f'Failed to load config {config_path}: {exc}. Using defaults.')

        self.declare_parameter('mqtt_host', str(config['mqtt_host']))
        self.declare_parameter('mqtt_port', int(config['mqtt_port']))
        self.declare_parameter('device_type', str(config['device_type']))
        self.declare_parameter('device_id', str(config['device_id']))
        self.declare_parameter('command_topic', str(config['command_topic']))

        self.mqtt_host = self.get_parameter('mqtt_host').get_parameter_value().string_value
        self.mqtt_port = self.get_parameter('mqtt_port').get_parameter_value().integer_value
        self.device_type = self.get_parameter('device_type').get_parameter_value().string_value
        self.device_id = self.get_parameter('device_id').get_parameter_value().string_value
        self.command_topic = self.get_parameter('command_topic').get_parameter_value().string_value
        self.status_topic = f'device/{self.device_type}/{self.device_id}/status'

        self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_disconnect = self.on_disconnect
        self.mqtt_client.on_message = self.on_message

        try:
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
        except Exception as exc:
            self.get_logger().error(f'Failed to connect MQTT: {exc}')
            self.mqtt_client = None

    def on_connect(self, client, userdata, flags, rc):
        if rc != 0:
            self.get_logger().error(f'MQTT connect failed: rc={rc}')
            return

        client.subscribe(self.command_topic, qos=1)
        self.get_logger().info(
            f'Heartbeat online. Subscribed: {self.command_topic} | Pub: {self.status_topic}'
        )

    def on_disconnect(self, client, userdata, rc):
        if rc == 0:
            self.get_logger().info('MQTT disconnected')
        else:
            self.get_logger().warn(f'MQTT disconnected unexpectedly: rc={rc}')

    def is_targeted_to_this_device(self, data):
        target_type = str(data.get('targetDeviceType', '')).strip()
        target_id = str(data.get('targetDeviceId', '')).strip()

        if target_type and target_type != self.device_type:
            return False
        if target_id and target_id != self.device_id:
            return False
        return True

    def on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode('utf-8'))
        except Exception as exc:
            self.get_logger().warn(f'Invalid MQTT payload ignored: {exc}')
            return

        action = str(data.get('action', '')).strip().lower()
        if action != 'ping':
            return

        if not self.is_targeted_to_this_device(data):
            self.get_logger().debug(
                f'PING ignored for target={data.get("targetDeviceType", "")}/{data.get("targetDeviceId", "")}'
            )
            return

        self.publish_pong(data)

    def publish_pong(self, data):
        if self.mqtt_client is None:
            self.get_logger().warn('MQTT disconnected, PONG skipped')
            return

        payload = {
            'eventType': 'PONG',
            'deviceType': self.device_type,
            'deviceId': self.device_id,
            'requestId': data.get('requestId', ''),
        }
        result = self.mqtt_client.publish(self.status_topic, json.dumps(payload), qos=1)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            self.get_logger().info(f'PONG sent: {payload}')
        else:
            self.get_logger().warn(f'PONG publish failed: rc={result.rc}')

    def destroy_node(self):
        if self.mqtt_client is not None:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        super().destroy_node()


def main():
    rclpy.init()
    node = RobotHeartbeatNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
