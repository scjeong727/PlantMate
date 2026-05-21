#!/usr/bin/env python3
import json

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from geometry_msgs.msg import PoseStamped


class RobotCommandNode(Node):
    def __init__(self):
        super().__init__('robot_command_node')

        # 1. 서버(내부 노드)로부터 ROS2 토픽(/plantmate/robot_command) 구독 설정
        self.create_subscription(String, '/plantmate/robot_command', self.on_command, 10)
        
        # 2. JetRover 내비게이션(Nav2) 목적지 발행자(Publisher) 생성
        self.nav_pub = self.create_publisher(PoseStamped, '/goal_pose', 10)

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

        # 로봇이 'water' 뿐만 아니라 'move' 명령도 허용하도록 추가
        self.valid_actions = {'water', 'move', 'MOVE'} 

        try:
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)

            # 노트북(MQTT)에서 날아온 메시지를 처리할 콜백 함수 연결
            self.mqtt_client.on_message = self.on_mqtt_message

            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()

            # 노트북(MQTT)에서 날리는 명령어를 직접 수신하기 위해 방 이름 구독
            self.mqtt_client.subscribe('/plantmate/robot_command')

            self.get_logger().info(f'MQTT connected! Subscribed to /plantmate/robot_command | Pub: {self.status_topic}')
        except Exception as e:
            self.mqtt_client = None
            self.get_logger().error(f'Failed to connect MQTT: {e}')

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

        # 명령 종류(action)를 가져옵니다. 
        action_raw = str(data.get('action', '')).strip()
        action = action_raw.lower() # 소문자로 통일해서 비교하기 쉽게 변경
        
        plant_id = data.get('plantId', 0)
        detail = str(data.get('detail', '')).strip()

        # 로봇 터미널 창에 INFO 로그가 실시간으로 출력됩니다.
        self.get_logger().info(
            f'Received command: plant_id={plant_id}, action={action_raw}, detail={detail}'
        )

        if action not in {'water', 'move'}:
            self.get_logger().warn(f'Unsupported action: {action_raw}')
            return

        # -------------------------------------------------------------
        # 명령을 받자마자 서버에 ACK 송신하는 로직
        # -------------------------------------------------------------
        ack_payload = {
            'eventType': 'COMMAND_RECEIVED',
            'message': action_raw,
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

        # -------------------------------------------------------------
        # 수신된 명령(action)에 따라 실제 로봇을 움직이는 분기 처리
        # -------------------------------------------------------------
        if action == 'move':
            target_x = 0.0
            target_y = 0.0
            
            # 서버가 보낸 detail 문자열 내부의 JSON을 한 번 더 파싱하여 x, y 추출
            if detail:
                try:
                    detail_json = json.loads(detail)
                    target_x = float(detail_json.get('x', 0.0))
                    target_y = float(detail_json.get('y', 0.0))
                except Exception as e:
                    self.get_logger().error(f'좌표 파싱 실패: {e}')
            
            # Nav2 자율주행 스택에 보낼 규격 메시지 생성
            goal_msg = PoseStamped()
            goal_msg.header.frame_id = 'map'
            goal_msg.header.stamp = self.get_clock().now().to_msg()
            goal_msg.pose.position.x = target_x
            goal_msg.pose.position.y = target_y
            goal_msg.pose.orientation.w = 1.0 # 기본 방향
            
            # 로봇 목적지 좌표 발행
            self.nav_pub.publish(goal_msg)
            self.get_logger().info(f'[자율주행 시작] 로봇이 다음 좌표로 이동합니다 -> X: {target_x}, Y: {target_y}')
            
        elif action == 'water':
            self.get_logger().info('Command accepted for ROB-001 only: water (물주기 액션 실행 대기)')

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
