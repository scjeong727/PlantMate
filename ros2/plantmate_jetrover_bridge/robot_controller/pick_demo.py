#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from ros_robot_controller_msgs.msg import (
    ServoPosition, ServosPosition,
    SetBusServoState, BusServoState
)

TOPIC_POS = "/ros_robot_controller/bus_servo/set_position"
TOPIC_STATE = "/ros_robot_controller/bus_servo/set_state"
SERVO_IDS = [1, 2, 3, 4, 5, 10]

STEPS = [
    {"time_ms": 1500, "pos": {1: 900, 2: 500, 3: 150, 4: 500, 5: 500, 10: 15}},
    {"time_ms": 1500, "pos": {1: 900, 2: 100, 3: 150, 4: 900, 5: 500, 10: 15}},
    {"time_ms": 800,  "pos": {1: 900, 2: 150, 3: 150, 4: 800, 5: 500, 10: 15}},
    {"time_ms": 800,  "pos": {1: 900, 2: 100, 3: 300, 4: 700, 5: 500, 10: 15}},
    {"time_ms": 800,  "pos": {1: 900, 2: 100, 3: 300, 4: 700, 5: 500, 10: 200}},
    {"time_ms": 800,  "pos": {1: 900, 2: 50,  3: 400, 4: 750, 5: 500, 10: 50}},
    {"time_ms": 800,  "pos": {1: 900, 2: 50,  3: 400, 4: 650, 5: 500, 10: 800}},
    {"time_ms": 500,  "pos": {1: 900, 2: 200, 3: 250, 4: 750, 5: 500, 10: 900}},
    {"time_ms": 1500, "pos": {1: 500, 2: 650, 3: 15,  4: 500, 5: 500, 10: 900}},
]

class ActionGroupRunner(Node):
    def __init__(self):
        super().__init__("pick_demo_runner")
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
        while rclpy.ok() and not node.finished:
            rclpy.spin_once(node, timeout_sec=0.1)
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()

if __name__ == "__main__":
    main()
