#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from ros_robot_controller_msgs.msg import ServoPosition, ServosPosition

TOPIC = "/ros_robot_controller/bus_servo/set_position"

STEPS = [
    {"time_ms": 500,  "pos": {1: 875, 2: 550, 3: 300, 4: 300, 5: 500, 10: 900}},
    {"time_ms": 1500, "pos": {1: 875, 2: 195, 3: 300, 4: 700, 5: 500, 10: 900}},
    {"time_ms": 200,  "pos": {1: 875, 2: 195, 3: 300, 4: 600, 5: 500, 10: 900}},
    {"time_ms": 500,  "pos": {1: 875, 2: 195, 3: 300, 4: 600, 5: 500, 10: 900}},
    {"time_ms": 200,  "pos": {1: 875, 2: 195, 3: 300, 4: 500, 5: 500, 10: 50}},
    {"time_ms": 500,  "pos": {1: 875, 2: 300, 3: 150, 4: 300, 5: 500, 10: 450}},
    {"time_ms": 1500, "pos": {1: 500, 2: 750, 3: 0,   4: 375, 5: 500, 10: 500}},
]

class ActionGroupRunner(Node):
    def __init__(self):
        super().__init__("action_group_runner")
        self.pub = self.create_publisher(ServosPosition, TOPIC, 1)
        self.idx = 0
        self.timer = None
        self.run_next_step()

    def run_next_step(self):
        if self.idx >= len(STEPS):
            self.get_logger().info("done")
            rclpy.shutdown()
            return

        step = STEPS[self.idx]
        msg = ServosPosition()
        msg.duration = float(step["time_ms"]) / 1000.0  # 핵심: ms -> s

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
        dt = max(msg.duration, 0.02)
        self.timer = self.create_timer(dt, self._timer_cb)

    def _timer_cb(self):
        if self.timer is not None:
            self.timer.cancel()
            self.timer = None
        self.run_next_step()

def main():
    rclpy.init()
    node = ActionGroupRunner()
    rclpy.spin(node)

if __name__ == "__main__":
    main()





