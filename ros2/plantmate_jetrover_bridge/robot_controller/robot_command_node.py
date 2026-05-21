#!/usr/bin/env python3
import json
import math
from pathlib import Path

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from geometry_msgs.msg import PoseStamped, Twist
from nav_msgs.msg import Odometry


DEFAULT_CONFIG = {
    'mqtt_host': '192.168.0.6',
    'mqtt_port': 1883,
    'device_type': 'arm',
    'device_id': 'robot-1',
    'command_topic': '/plantmate/robot_command',
    'ros_command_topic': '/plantmate/robot_command',
    'nav_goal_topic': '/goal_pose',
    'odom_topic': '/odom',
    'cmd_vel_topic': '/cmd_vel',
    'goal_tolerance': 0.15,
    'angle_tolerance': 0.2,
    'linear_gain': 0.35,
    'angular_gain': 1.2,
    'max_linear_speed': 0.25,
    'max_angular_speed': 0.8,
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


def parse_detail(detail):
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
        if '=' not in part:
            continue
        key, value = part.split('=', 1)
        parsed[key.strip()] = value.strip()
    return parsed


def normalize_angle(angle):
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def yaw_from_quaternion(q):
    siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny_cosp, cosy_cosp)


def clamp(value, min_value, max_value):
    return max(min_value, min(max_value, value))


class RobotCommandNode(Node):
    def __init__(self):
        super().__init__('robot_command_node')

        self.declare_parameter(
            'config_path',
            str(Path(__file__).with_name('robot_config.json')),
        )
        config_path = Path(self.get_parameter('config_path').get_parameter_value().string_value)
        try:
            config = load_config(config_path)
            self.get_logger().info(f'Loaded config: {config_path}')
        except Exception as e:
            config = dict(DEFAULT_CONFIG)
            self.get_logger().error(f'Failed to load config {config_path}: {e}. Using defaults.')

        self.declare_parameter('mqtt_host', str(config['mqtt_host']))
        self.declare_parameter('mqtt_port', int(config['mqtt_port']))
        self.declare_parameter('device_type', str(config['device_type']))
        self.declare_parameter('device_id', str(config['device_id']))
        self.declare_parameter('command_topic', str(config['command_topic']))
        self.declare_parameter('ros_command_topic', str(config['ros_command_topic']))
        self.declare_parameter('nav_goal_topic', str(config['nav_goal_topic']))
        self.declare_parameter('odom_topic', str(config['odom_topic']))
        self.declare_parameter('cmd_vel_topic', str(config['cmd_vel_topic']))
        self.declare_parameter('goal_tolerance', float(config['goal_tolerance']))
        self.declare_parameter('angle_tolerance', float(config['angle_tolerance']))
        self.declare_parameter('linear_gain', float(config['linear_gain']))
        self.declare_parameter('angular_gain', float(config['angular_gain']))
        self.declare_parameter('max_linear_speed', float(config['max_linear_speed']))
        self.declare_parameter('max_angular_speed', float(config['max_angular_speed']))

        self.mqtt_host = self.get_parameter('mqtt_host').get_parameter_value().string_value
        self.mqtt_port = self.get_parameter('mqtt_port').get_parameter_value().integer_value
        self.device_type = self.get_parameter('device_type').get_parameter_value().string_value
        self.device_id = self.get_parameter('device_id').get_parameter_value().string_value
        self.command_topic = self.get_parameter('command_topic').get_parameter_value().string_value
        self.ros_command_topic = self.get_parameter('ros_command_topic').get_parameter_value().string_value
        self.nav_goal_topic = self.get_parameter('nav_goal_topic').get_parameter_value().string_value
        self.odom_topic = self.get_parameter('odom_topic').get_parameter_value().string_value
        self.cmd_vel_topic = self.get_parameter('cmd_vel_topic').get_parameter_value().string_value
        self.goal_tolerance = self.get_parameter('goal_tolerance').get_parameter_value().double_value
        self.angle_tolerance = self.get_parameter('angle_tolerance').get_parameter_value().double_value
        self.linear_gain = self.get_parameter('linear_gain').get_parameter_value().double_value
        self.angular_gain = self.get_parameter('angular_gain').get_parameter_value().double_value
        self.max_linear_speed = self.get_parameter('max_linear_speed').get_parameter_value().double_value
        self.max_angular_speed = self.get_parameter('max_angular_speed').get_parameter_value().double_value

        self.current_x = 0.0
        self.current_y = 0.0
        self.current_yaw = 0.0
        self.has_odom = False
        self.active_goal = None

        # 1. 서버(내부 노드)로부터 ROS2 토픽(/plantmate/robot_command) 구독 설정
        self.create_subscription(String, self.ros_command_topic, self.on_command, 10)
        
        # 2. JetRover 내비게이션(Nav2) 목적지 발행자(Publisher) 생성
        self.nav_pub = self.create_publisher(PoseStamped, self.nav_goal_topic, 10)
        self.cmd_vel_pub = self.create_publisher(Twist, self.cmd_vel_topic, 10)
        self.create_subscription(Odometry, self.odom_topic, self.on_odom, 10)
        self.create_timer(0.1, self.control_loop)

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
            self.mqtt_client.subscribe(self.command_topic)

            self.get_logger().info(f'MQTT connected! Subscribed to {self.command_topic} | Pub: {self.status_topic}')
        except Exception as e:
            self.mqtt_client = None
            self.get_logger().error(f'Failed to connect MQTT: {e}')

    def on_odom(self, msg):
        pose = msg.pose.pose
        self.current_x = pose.position.x
        self.current_y = pose.position.y
        self.current_yaw = yaw_from_quaternion(pose.orientation)
        self.has_odom = True

    def publish_stop(self):
        self.cmd_vel_pub.publish(Twist())

    def set_navigation_goal(self, target_x, target_y, action, plant_id=0, duration=''):
        self.active_goal = {
            'x': target_x,
            'y': target_y,
            'action': action,
            'plant_id': plant_id,
            'duration': duration,
        }

        goal_msg = PoseStamped()
        goal_msg.header.frame_id = 'map'
        goal_msg.header.stamp = self.get_clock().now().to_msg()
        goal_msg.pose.position.x = target_x
        goal_msg.pose.position.y = target_y
        goal_msg.pose.orientation.w = 1.0
        self.nav_pub.publish(goal_msg)

    def control_loop(self):
        if self.active_goal is None:
            return

        if not self.has_odom:
            self.get_logger().warn('odom 수신 전이라 이동 제어를 대기합니다.')
            return

        target_x = self.active_goal['x']
        target_y = self.active_goal['y']
        dx = target_x - self.current_x
        dy = target_y - self.current_y
        distance = math.hypot(dx, dy)

        if distance <= self.goal_tolerance:
            finished = self.active_goal
            self.active_goal = None
            self.publish_stop()
            self.get_logger().info(
                f'[목표 도착] action={finished["action"]}, plant_id={finished["plant_id"]}, '
                f'X: {target_x}, Y: {target_y}'
            )
            if finished['action'] == 'water':
                self.get_logger().info(
                    f'[물주기 실행] plant_id={finished["plant_id"]}, duration={finished["duration"]}'
                )
            return

        target_yaw = math.atan2(dy, dx)
        yaw_error = normalize_angle(target_yaw - self.current_yaw)

        cmd = Twist()
        cmd.angular.z = clamp(
            self.angular_gain * yaw_error,
            -self.max_angular_speed,
            self.max_angular_speed,
        )

        if abs(yaw_error) <= self.angle_tolerance:
            cmd.linear.x = clamp(
                self.linear_gain * distance,
                0.0,
                self.max_linear_speed,
            )

        self.cmd_vel_pub.publish(cmd)

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
            detail_data = parse_detail(detail)
            try:
                target_x = float(detail_data.get('x', 0.0))
                target_y = float(detail_data.get('y', 0.0))
            except Exception as e:
                self.get_logger().error(f'좌표 파싱 실패: {e}')
                return

            self.set_navigation_goal(target_x, target_y, 'move', plant_id)
            self.get_logger().info(f'[자율주행 시작] 로봇이 다음 좌표로 이동합니다 -> X: {target_x}, Y: {target_y}')

        elif action == 'water':
            detail_data = parse_detail(detail)
            duration = detail_data.get('duration', '')

            if 'x' in detail_data and 'y' in detail_data:
                try:
                    target_x = float(detail_data.get('x', 0.0))
                    target_y = float(detail_data.get('y', 0.0))
                except Exception as e:
                    self.get_logger().error(f'좌표 파싱 실패: {e}')
                    return

                self.set_navigation_goal(target_x, target_y, 'water', plant_id, duration)
                self.get_logger().info(
                    f'[물주기 이동 시작] plant_id={plant_id}, X: {target_x}, Y: {target_y}, duration={duration}'
                )
            else:
                self.get_logger().warn(f'물주기 명령에 좌표가 없습니다: detail={detail}')

            self.get_logger().info(f'[물주기 명령 수신] plant_id={plant_id}, duration={duration}')

    def destroy_node(self):
        self.publish_stop()
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
