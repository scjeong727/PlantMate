#!/usr/bin/env python3
import json
<<<<<<< Updated upstream
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
    'use_start_pose_as_origin': True,
    'reset_origin_on_command': True,
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
        self.declare_parameter('use_start_pose_as_origin', bool(config['use_start_pose_as_origin']))
        self.declare_parameter('reset_origin_on_command', bool(config['reset_origin_on_command']))
=======
import os
import re
import shlex
import subprocess
import sys

import paho.mqtt.client as mqtt
import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from std_msgs.msg import String


class RobotCommandNode(Node):
    WORKFLOW_TAG = 'water-seq-pick-place-v1'

    def __init__(self):
        super().__init__('robot_command_node')

        # 1. ROS2 토픽(/plantmate/robot_command) 구독 설정
        self.create_subscription(String, '/plantmate/robot_command', self.on_command, 10)

        # 2. JetRover 내비게이션(Nav2) 목적지 발행자(Publisher) 생성
        self.nav_pub = self.create_publisher(PoseStamped, '/goal_pose', 10)

        # 네트워크 설정
        self.declare_parameter('mqtt_host', '192.168.0.30')
        self.declare_parameter('mqtt_port', 1883)
        self.declare_parameter('device_type', 'arm')
        self.declare_parameter('device_id', 'robot-1')

        # water 실행 템플릿
        self.declare_parameter(
            'water_command',
            'python3 /home/ubuntu/run_watering.py --duration {duration}'
        )
        # arm 동작 템플릿
        self.declare_parameter(
            'arm_action_group',
            '/home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/action_groups/watering_demo.d6a'
        )
        self.declare_parameter(
            'arm_action_group_command',
            'python3 /home/ubuntu/run_action_group.py {action_file}'
        )
        self.declare_parameter(
            'arm_action_groups_dir',
            '/home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/action_groups'
        )
>>>>>>> Stashed changes

        self.mqtt_host = self.get_parameter('mqtt_host').get_parameter_value().string_value
        self.mqtt_port = self.get_parameter('mqtt_port').get_parameter_value().integer_value
        self.device_type = self.get_parameter('device_type').get_parameter_value().string_value
        self.device_id = self.get_parameter('device_id').get_parameter_value().string_value
<<<<<<< Updated upstream
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
        self.use_start_pose_as_origin = self.get_parameter('use_start_pose_as_origin').get_parameter_value().bool_value
        self.reset_origin_on_command = self.get_parameter('reset_origin_on_command').get_parameter_value().bool_value

        self.current_x = 0.0
        self.current_y = 0.0
        self.current_yaw = 0.0
        self.raw_x = 0.0
        self.raw_y = 0.0
        self.raw_yaw = 0.0
        self.has_odom = False
        self.origin_x = 0.0
        self.origin_y = 0.0
        self.origin_yaw = 0.0
        self.has_origin = False
        self.active_goal = None

        # 1. 서버(내부 노드)로부터 ROS2 토픽(/plantmate/robot_command) 구독 설정
        self.create_subscription(String, self.ros_command_topic, self.on_command, 10)
        
        # 2. JetRover 내비게이션(Nav2) 목적지 발행자(Publisher) 생성
        self.nav_pub = self.create_publisher(PoseStamped, self.nav_goal_topic, 10)
        self.cmd_vel_pub = self.create_publisher(Twist, self.cmd_vel_topic, 10)
        self.create_subscription(Odometry, self.odom_topic, self.on_odom, 10)
        self.create_timer(0.1, self.control_loop)

        self.status_topic = f'device/{self.device_type}/{self.device_id}/status'

        # 로봇 이동 제어 노드는 move/water만 처리합니다. ping/pong은 heartbeat 노드가 담당합니다.
        self.valid_actions = {'water', 'move'}

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
        self.raw_x = pose.position.x
        self.raw_y = pose.position.y
        self.raw_yaw = yaw_from_quaternion(pose.orientation)

        if self.use_start_pose_as_origin and not self.has_origin:
            self.reset_origin('현재 위치를 기준 원점으로 설정')

        self.update_relative_pose()
        self.has_odom = True

    def reset_origin(self, reason):
        self.origin_x = self.raw_x
        self.origin_y = self.raw_y
        self.origin_yaw = self.raw_yaw
        self.has_origin = True
        self.update_relative_pose()
        self.get_logger().info(
            f'{reason}: x={self.origin_x:.3f}, y={self.origin_y:.3f}, yaw={self.origin_yaw:.3f}'
        )

    def update_relative_pose(self):
        if self.use_start_pose_as_origin and self.has_origin:
            dx = self.raw_x - self.origin_x
            dy = self.raw_y - self.origin_y
            cos_yaw = math.cos(-self.origin_yaw)
            sin_yaw = math.sin(-self.origin_yaw)
            self.current_x = dx * cos_yaw - dy * sin_yaw
            self.current_y = dx * sin_yaw + dy * cos_yaw
            self.current_yaw = normalize_angle(self.raw_yaw - self.origin_yaw)
        else:
            self.current_x = self.raw_x
            self.current_y = self.raw_y
            self.current_yaw = self.raw_yaw

    def prepare_command_origin(self):
        if not self.reset_origin_on_command:
            return True

        if not self.has_odom:
            self.get_logger().warn('odom 수신 전이라 명령 기준 원점을 설정할 수 없습니다.')
            return False

        self.reset_origin('명령 수신 시 현재 위치를 기준 원점으로 재설정')
        return True

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

    def is_targeted_to_this_device(self, data):
        target_type = str(data.get('targetDeviceType', '')).strip()
        target_id = str(data.get('targetDeviceId', '')).strip()

        if target_type and target_type != self.device_type:
            return False
        if target_id and target_id != self.device_id:
            return False
        return True

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

        if not self.is_targeted_to_this_device(data):
            self.get_logger().info(
                f'Command ignored for target={data.get("targetDeviceType", "")}/{data.get("targetDeviceId", "")}'
            )
            return

        if action == 'ping':
            self.get_logger().debug('PING ignored by robot_command_node; handled by robot_heartbeat_node')
            return

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

            if not self.prepare_command_origin():
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

                if not self.prepare_command_origin():
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
=======
        self.water_command = self.get_parameter('water_command').get_parameter_value().string_value
        self.arm_action_group = self.get_parameter('arm_action_group').get_parameter_value().string_value
        self.arm_action_group_command = self.get_parameter('arm_action_group_command').get_parameter_value().string_value
        self.arm_action_groups_dir = self.get_parameter('arm_action_groups_dir').get_parameter_value().string_value

        self.status_topic = f'device/{self.device_type}/{self.device_id}/status'

        self.valid_actions = {'water', 'watering', 'move', 'arm'}
        self.arm_actions_map = {
            'water': os.path.join(self.arm_action_groups_dir, 'watering_demo.d6a'),
            'watering': os.path.join(self.arm_action_groups_dir, 'watering_demo.d6a'),
            'watering_demo': os.path.join(self.arm_action_groups_dir, 'watering_demo.d6a'),
            'watering_end': os.path.join(self.arm_action_groups_dir, 'watering_end_demo.d6a'),
            'watering_end_demo': os.path.join(self.arm_action_groups_dir, 'watering_end_demo.d6a'),
            'pick': os.path.join(self.arm_action_groups_dir, 'pick_demo.d6a'),
            'pick_demo': os.path.join(self.arm_action_groups_dir, 'pick_demo.d6a'),
            'arm_forward': os.path.join(self.arm_action_groups_dir, 'arm_forward.d6a'),
            'forward': os.path.join(self.arm_action_groups_dir, 'arm_forward.d6a'),
        }

        try:
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
            self.mqtt_client.on_message = self.on_mqtt_message
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            self.mqtt_client.subscribe('/plantmate/robot_command')
            self.mqtt_client.subscribe('plantmate/robot_command')
            self.get_logger().info(
                f'MQTT connected! Subscribed to /plantmate/robot_command and plantmate/robot_command | Pub: {self.status_topic}'
            )
            self.get_logger().info(f'Workflow tag: {self.WORKFLOW_TAG}')
            self.get_logger().info(f'Node source: {__file__}')
            self.get_logger().info(f'Python executable: {sys.executable}')
            self.get_logger().info(f'Water command configured: {self.water_command}')
            self.get_logger().info(f'Arm action-group command configured: {self.arm_action_group_command}')
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

        action_raw = str(data.get('action', '')).strip()
        action = action_raw.lower()
        plant_id = data.get('plantId', 0)
        detail = data.get('detail', '')
        detail_payload = self._parse_detail(detail)

        self.get_logger().info(
            f'Received command: plant_id={plant_id}, action={action_raw}, detail={detail}'
        )

        if action not in self.valid_actions:
            self.get_logger().warn(f'Unsupported action: {action_raw}')
            return

        ack_payload = {
            'eventType': 'COMMAND_RECEIVED',
            'message': action_raw,
            'plantId': plant_id,
            'detail': detail,
        }
        if self.mqtt_client is not None:
            result = self.mqtt_client.publish(self.status_topic, json.dumps(ack_payload), qos=1)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                self.get_logger().info(f'ACK sent successfully with QoS 1: {ack_payload}')
            else:
                self.get_logger().warn(f'ACK publish failed: rc={result.rc}')
        else:
            self.get_logger().warn(f'MQTT disconnected, ACK skipped: {ack_payload}')

        if action == 'move':
            self.get_logger().info('이동명령 처리 금지')
        elif action == 'watering':
            duration = self._extract_duration(detail_payload, detail)
            self.get_logger().info(f'[WATER] received command -> plant_id={plant_id}, duration={duration}, detail={detail}')
            self._run_water_sequence(duration)
        elif action == 'water':
            duration = self._extract_duration(detail_payload, detail)
            self.get_logger().info(f'[WATER] received command -> plant_id={plant_id}, duration={duration}, detail={detail}')
            self._run_water_sequence(duration)
        elif action == 'arm':
            action_file = self._resolve_arm_action_file(detail_payload, detail)
            cmd = self._build_command(self.arm_action_group_command, action_file=action_file)
            self.get_logger().info(f'[ARM] direct command -> action_file={action_file}')
            self._run_external_command('ARM', cmd)

    def _parse_detail(self, detail):
        if isinstance(detail, dict):
            return detail
        if not detail:
            return {}
        if not isinstance(detail, str):
            detail = str(detail).strip()
        try:
            parsed = json.loads(detail)
            if isinstance(parsed, dict):
                return parsed
        except Exception:
            pass
        detail_pairs = {}
        parts = re.split(r'[,;\s]+', detail)
        for part in parts:
            part = part.strip()
            if not part or '=' not in part:
                continue
            key, value = part.split('=', 1)
            detail_pairs[key.strip()] = value.strip().strip('"\'')
        return detail_pairs

    def _extract_duration(self, detail_payload, detail):
        raw = detail_payload.get('duration')
        if raw is None:
            raw = detail
        if isinstance(raw, (int, float)):
            return raw
        try:
            return int(str(raw))
        except Exception:
            return 5

    def _run_water_sequence(self, duration):
        self.get_logger().info('[WATER] starting integrated sequence: pick -> water -> watering_end')
        # 물주기 실행 전/후로 팔 동작을 함께 수행하는 고정 시퀀스
        # 1) 물병 집기
        pick_file = self._resolve_arm_action_file({'action': 'pick'}, '')
        pick_cmd = self._build_command(self.arm_action_group_command, action_file=pick_file)
        self.get_logger().info(f'[WATER] step1 pick action_file={pick_file}')
        if not self._run_external_command('ARM', pick_cmd):
            self.get_logger().error('[WATER] 물병 집기 실패 -> 전체 시퀀스 중단')
            return

        # 2) 물주기
        water_cmd = self._build_command(self.water_command, duration=duration)
        self.get_logger().info(f'[WATER] step2 watering duration={duration}')
        if not self._run_external_command('WATER', water_cmd):
            self.get_logger().error('[WATER] 물주기 실패 -> 물병 놓기 건너뜀')
            return

        # 3) 물병 놓기
        place_file = self._resolve_arm_action_file({'action': 'watering_end'}, '')
        place_cmd = self._build_command(self.arm_action_group_command, action_file=place_file)
        self.get_logger().info(f'[WATER] step3 place action_file={place_file}')
        self._run_external_command('ARM', place_cmd)

    def _resolve_arm_action_file(self, detail_payload, detail):
        candidates = [
            detail_payload.get('action_file'),
            detail_payload.get('file'),
            detail_payload.get('path'),
            detail_payload.get('action'),
            detail_payload.get('name'),
        ]
        file_name = None
        for candidate in candidates:
            if candidate:
                file_name = str(candidate).strip()
                break
        if not file_name:
            file_name = detail.strip()
        if not file_name:
            return self.arm_action_group

        file_name = file_name.strip()
        mapped = self.arm_actions_map.get(file_name)
        if mapped:
            return mapped

        lower_name = file_name.lower()
        mapped = self.arm_actions_map.get(lower_name)
        if mapped:
            return mapped

        if os.path.sep not in file_name and not lower_name.endswith('.d6a'):
            mapped = self.arm_actions_map.get(lower_name.replace('.txt', ''))
            if mapped:
                return mapped

        if not os.path.isabs(file_name):
            if file_name.endswith('.d6a'):
                file_name = os.path.join(self.arm_action_groups_dir, file_name)
            else:
                file_name = os.path.join(self.arm_action_groups_dir, f'{file_name}.d6a')

        if not os.path.exists(file_name):
            self.get_logger().warn(f'Action group file not found: {file_name}')
        return file_name

    def _build_command(self, command_template, **kwargs):
        try:
            return command_template.format(**kwargs)
        except Exception as e:
            self.get_logger().error(f'Command template parse failed: {e}')
            raise

    def _run_external_command(self, label, command):
        try:
            args = shlex.split(command)
            self.get_logger().info(f'[{label}] execute: {args}')
            proc = subprocess.run(
                args,
                check=False,
                capture_output=True,
                text=True,
                timeout=120,
            )
            if proc.returncode == 0:
                self.get_logger().info(f'[{label}] {label.lower()} command completed successfully')
                return True
            else:
                self.get_logger().error(
                    f'[{label}] {label.lower()} command failed rc={proc.returncode}, '
                    f'stdout={proc.stdout}, stderr={proc.stderr}'
                )
                return False
        except subprocess.TimeoutExpired as e:
            self.get_logger().error(f'[{label}] command timeout: {e}')
            return False
        except FileNotFoundError:
            self.get_logger().error(f'[{label}] command failed: executable not found')
            return False
        except Exception as e:
            self.get_logger().error(f'[{label}] command failed: {e}')
            return False

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
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()

>>>>>>> Stashed changes
