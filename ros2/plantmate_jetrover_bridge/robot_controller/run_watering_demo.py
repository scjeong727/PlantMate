#!/usr/bin/env python3
import rclpy
import time
from rclpy.node import Node
from ros_robot_controller_msgs.msg import (
    ServoPosition, ServosPosition,
    SetBusServoState, BusServoState
)

TOPIC_POS = "/ros_robot_controller/bus_servo/set_position"
TOPIC_STATE = "/ros_robot_controller/bus_servo/set_state"
SERVO_IDS = [1, 2, 3, 4, 5, 10]

STEPS = [
    {"time_ms": 1500,  "pos": {1: 875, 2: 550, 3: 250, 4: 300, 5: 500, 10: 900}},
    {"time_ms": 500,  "pos": {1: 875, 2: 500, 3: 200, 4: 250, 5: 500, 10: 900}},
    {"time_ms": 200,  "pos": {1: 875, 2: 450, 3: 200, 4: 150, 5: 500, 10: 900}},
    {"time_ms": 3000, "pos": {1: 875, 2: 450, 3: 250, 4: 50,  5: 500, 10: 900}},
    {"time_ms": 500,  "pos": {1: 875, 2: 450, 3: 200, 4: 150, 5: 500, 10: 900}},
    {"time_ms": 500,  "pos": {1: 875, 2: 550, 3: 250, 4: 250, 5: 500, 10: 900}},
    {"time_ms": 1500, "pos": {1: 875, 2: 550, 3: 300, 4: 300, 5: 500, 10: 900}},
    {"time_ms": 1000, "pos": {1: 500, 2: 750, 3: 0,   4: 375, 5: 500, 10: 900}},
]

class ActionGroupRunner(Node):
    def __init__(self):
        super().__init__("watering_demo_runner")
        self.pub = self.create_publisher(ServosPosition, TOPIC_POS, 1)
        self.state_pub = self.create_publisher(SetBusServoState, TOPIC_STATE, 1)
        self.idx = 0
        self.timer = None
        self.finished = False

        self.publish_torque(True)  # ON
        self.run_next_step()

    def publish_torque(self, on: bool):
        msg = SetBusServoState()
        st = BusServoState()
        st.target_id = SERVO_IDS
        st.enable_torque = [1 if on else 0] * len(SERVO_IDS)
        msg.state = [st]
        msg.duration = 0.0
        self.state_pub.publish(msg)

    def run_next_step(self):
        if self.idx >= len(STEPS):
            self.get_logger().info("done")
            self.finished = True
            return

        step = STEPS[self.idx]
        msg = ServosPosition()
        msg.duration = float(step["time_ms"]) / 1000.0

        plist = []
        for sid, p in step["pos"].items():
            sp = ServoPosition()
            sp.id = int(sid)
            sp.position = int(p)
            plist.append(sp)
        msg.position = plist

        self.pub.publish(msg)
        self.get_logger().info(f"step={self.idx+1}, duration={msg.duration}s")
        self.idx += 1
        self.timer = self.create_timer(max(msg.duration, 0.02), self._timer_cb)

    def _timer_cb(self):
        if self.timer is not None:
            self.timer.cancel()
            self.timer = None
        self.run_next_step()

def main():
    rclpy.init()
    node = ActionGroupRunner()
    try:
        # 모든 동작 스텝 수행
        while rclpy.ok() and not node.finished:
            rclpy.spin_once(node, timeout_sec=0.1)
        
        # 중요: 동작 완료 후 마지막 토픽 데이터가 하드웨어 버퍼를 통해 완전히 전송되도록 잔여 스핀 수행
        if rclpy.ok():
            node.get_logger().info("Finishing node connection safely...")
            for _ in range(5):
                rclpy.spin_once(node, timeout_sec=0.1)
                
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()

if __name__ == "__main__":
    main()
