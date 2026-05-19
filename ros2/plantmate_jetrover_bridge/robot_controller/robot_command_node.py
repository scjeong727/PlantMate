#!/usr/bin/env python3
import json

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class RobotCommandNode(Node):
    def __init__(self):
        super().__init__('robot_command_node')

        # 1. 서버(내부 노드)로부터 ROS2 토픽(/plantmate/robot_command) 구독 설정
        self.create_subscription(String, '/plantmate/robot_command', self.on_command, 10)

        # [네트워크 설정] 노트북 PC(WSL)의 가상 IP 주소
        self.declare_parameter('mqtt_host', '192.168.0.4')
        self.declare_parameter('mqtt_port', 1883)
        self.declare_parameter('device_type', 'arm')
        self.declare_parameter('device_id', 'robot-1')

        self.mqtt_host = self.get_parameter('mqtt_host').get_parameter_value().string_value
        self.mqtt_port = self.get_parameter('mqtt_port').get_parameter_value().integer_value
        self.device_type = self.get_parameter('device_type').get_parameter_value().string_value
        self.device_id = self.get_parameter('device_id').get_parameter_value().string_value

        self.status_topic = f'device/{self.device_type}/{self.device_id}/status'

        self.valid_actions = {'water'}

        try:
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)

            # [추가된 핵심 기능 1] 노트북(MQTT)에서 날아온 메시지를 처리할 콜백 함수 연결
            self.mqtt_client.on_message = self.on_mqtt_message

            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()

            # [추가된 핵심 기능 2] 노트북(MQTT)에서 날리는 명령어를 직접 수신하기 위해 방 이름 구독
            self.mqtt_client.subscribe('/plantmate/robot_command')

            self.get_logger().info(f'MQTT connected! Subscribed to /plantmate/robot_command | Pub: {self.status_topic}')
        except Exception as e:
            self.mqtt_client = None
            self.get_logger().error(f'Failed to connect MQTT: {e}')

    # [추가된 핵심 기능 3] 노트북에서 쏜 MQTT 메시지를 가공해서 기존 로직(ROS2)으로 넘겨주는 다리 함수
    def on_mqtt_message(self, client, userdata, msg):
        try:
            payload_str = msg.payload.decode('utf-8')

            ros2_msg = String()
            ros2_msg.data = payload_str
            self.on_command(ros2_msg)
        except Exception as e:
            self.get_logger().error(f'Failed to process MQTT message: {e}')

    def on_command(self, msg):
        try:
            data = json.loads(msg.data)
        except Exception as e:
            self.get_logger().error(f'Invalid command json: {e}')
            return

        action = str(data.get('action', '')).strip()
        plant_id = data.get('plantId', 0)
        detail = str(data.get('detail', '')).strip()

        # 로봇 터미널 창에 INFO 로그가 실시간으로 출력됩니다.
        self.get_logger().info(
            f'Received command: plant_id={plant_id}, action={action}, detail={detail}'
        )

        if action not in self.valid_actions:
            self.get_logger().warn(f'Unsupported action: {action}')
            return

        if not isinstance(plant_id, int) or plant_id <= 0:
            self.get_logger().warn(f'Invalid plantId: {plant_id}')
            return

        ack_payload = {
            'eventType': 'COMMAND_RECEIVED',
            'message': action,
            'plantId': plant_id,
            'detail': detail,
        }

        if self.mqtt_client is not None:
            result = self.mqtt_client.publish(
                self.status_topic,
                json.dumps(ack_payload),
                qos=1,
            )

            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                self.get_logger().info(f'ACK sent successfully with QoS 1: {ack_payload}')
            else:
                self.get_logger().warn(f'ACK publish failed: rc={result.rc}')
        else:
            self.get_logger().warn(f'MQTT disconnected, ACK skipped: {ack_payload}')

        self.get_logger().info(f'Command accepted for ROB-001 only: {action}')

    def destroy_node(self):
        if self.mqtt_client is not None:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        super().destroy_node()


def main():
    rclpy.init()
    node = RobotCommandNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()