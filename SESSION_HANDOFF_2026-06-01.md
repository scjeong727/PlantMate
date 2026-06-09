# PlantMate Session Handoff - 2026-06-01

## Workspace

Local root:

```text
/mnt/c/Users/user/DeploymentProgram
```

Branch:

```text
develop
```

Starting remote baseline for this pass:

```text
a0d9c8b4fbff363acab9a084ff5c328b67b9bdad
Update Watering action value
```

That commit only changed servo/action values in:

```text
pick_demo.py
run_watering_demo.py
run_watering_end_demo.py
```

It did not change `robot_command_node.py`, `robot_heartbeat_node.py`,
`run_move_demo.py`, MQTT routing, or the movement algorithm.

## Current Goal State

The project is trying to run this robot watering flow:

```text
Android
-> C server MQTT broker on 192.168.0.6:1883
-> JetRover Docker container `jetrover`
-> ROS2 scripts under /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
```

Required runtime terminals on JetRover:

```bash
docker exec -it jetrover bash
source /opt/ros/humble/setup.bash
source /home/ubuntu/ros2_ws/install/setup.bash
ros2 launch controller controller.launch.py
```

```bash
docker exec -it jetrover bash
source /opt/ros/humble/setup.bash
source /home/ubuntu/ros2_ws/install/setup.bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_heartbeat_node.py
```

```bash
docker exec -it jetrover bash
source /opt/ros/humble/setup.bash
source /home/ubuntu/ros2_ws/install/setup.bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_command_node.py
```

## JetRover Access

SSH works with key authentication:

```bash
ssh hiwonder@192.168.0.5
```

The ROS2 runtime is inside Docker:

```bash
docker ps
```

Expected container:

```text
NAMES: jetrover
IMAGE: ros:humble
```

The active target folder inside the container is:

```text
/home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
```

Deploy from PC/WSL to JetRover host:

```bash
scp -r /mnt/c/Users/user/DeploymentProgram/ros2/plantmate_jetrover_bridge/robot_controller \
  hiwonder@192.168.0.5:/home/hiwonder/
```

Copy from JetRover host to Docker container:

```bash
docker cp /home/hiwonder/robot_controller/. \
  jetrover:/home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller/
```

Then inside the container:

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
chmod +x *.py
```

## MQTT / Online Detection

Device identity is standardized as:

```text
device_type=arm
device_id=robot-1
```

DB bindings that matter:

```text
plant_id=8 role=robot device_type=arm device_id=robot-1
plant_id=8 role=water device_type=arm device_id=robot-1
```

Heartbeat behavior:

```text
Server publishes PING every 3s to /plantmate/robot_command
robot_heartbeat_node.py publishes PONG to device/arm/robot-1/status
Server marks mqtt_live_devices arm/robot-1 online=1 on PONG
Server marks it offline after 10s stale timeout
```

Server logging was added for online/offline transitions:

```text
[mqtt] live device online: type=arm id=robot-1
[mqtt] live device stale/offline: type=arm id=robot-1 timeout=10s
```

Verified behavior during this pass:

```text
heartbeat stop/start works without restarting server
robot command returns ERROR robot_offline when mqtt_live_devices online=0
```

## Android MQTT Binding

`MqttPlantGateway.java` was changed so Android MQTT mode uses ROS2 device type:

```text
ROS2_DEVICE_TYPE = "arm"
```

Affected RPCs:

```text
loadWaterDevices -> getDeviceList deviceType=arm
setWaterDevice   -> bindDevice role=water deviceType=arm
loadRobotDevices -> getDeviceList deviceType=arm
setRobotDevice   -> bindDevice role=robot deviceType=arm
```

This is needed because heartbeat PONGs arrive as `arm/robot-1`, not
`robot/robot-1` or `pump/...`.

## ROS2 Reconnect Changes

`robot_heartbeat_node.py`:

```text
reconnect_delay_set(min_delay=1, max_delay=10)
```

`robot_command_node.py`:

```text
on_connect added
on_disconnect added
reconnect_delay_set(min_delay=1, max_delay=10)
re-subscribes on reconnect:
  /plantmate/robot_command
  device/arm/robot-1/move/command
  device/arm/robot-1/water/command
```

## Watering Movement Direction Change

The original `run_water_sequence.py` used `run_move_demo.py --x ... --y ...`
for both go and return. That caused coordinate-frame confusion because each
`run_move_demo.py` invocation re-established its own local origin/yaw.

The latest direction is to avoid one combined move controller for watering and
split the sequence into explicit turn/drive steps:

```text
1. pick_demo.py
2. run_turn_demo.py      --yaw atan2(y, x)
3. run_drive_demo.py     --distance hypot(x, y)
4. run_turn_demo.py      --yaw -atan2(y, x)
5. run_watering_demo.py
6. run_turn_demo.py      --yaw atan2(y, x)
7. run_drive_demo.py     --distance -hypot(x, y)
8. run_turn_demo.py      --yaw -atan2(y, x)
9. run_watering_end_demo.py
```

New files:

```text
run_turn_demo.py
run_drive_demo.py
```

Current known risk:

```text
run_turn_demo.py was just changed after a one-sided continuous rotation bug.
It now rotates in a fixed requested direction and stops/fails if odom progress
does not improve for 3 seconds. This exact fix still needs JetRover validation.
```

If rotation still spins the wrong way, likely the `/cmd_vel angular.z` sign and
`/odom` yaw sign are opposite on this robot. Add a config sign multiplier such
as `turn_direction_sign=-1` in `run_turn_demo.py`.

## Task Busy / Stuck Sequence

`robot_command_node.py` only allows one active task:

```text
active_task = water_sequence
```

While this is set, new commands log:

```text
task busy: water_sequence
```

Clear current stuck run:

```bash
pkill -f run_water_sequence.py
pkill -f run_move_demo.py
pkill -f run_turn_demo.py
pkill -f run_drive_demo.py
pkill -f run_watering_demo.py
pkill -f run_watering_end_demo.py
```

Send stop:

```bash
source /opt/ros/humble/setup.bash
source /home/ubuntu/ros2_ws/install/setup.bash
ros2 topic pub --once /controller/cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}"
```

Then restart `robot_command_node.py`.

## Safety Stop

If the robot keeps moving or rotating:

```bash
for i in {1..20}; do
  ros2 topic pub --once /controller/cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}"
  sleep 0.1
done
```

If it still moves, cut motor/robot power first. Debug after physical safety.

## Files Changed In This Handoff

Commit candidates from this pass:

```text
android/app/src/main/java/kr/ac/dju/plantmate/protocol/mqtt/MqttPlantGateway.java
ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py
ros2/plantmate_jetrover_bridge/robot_controller/robot_config.json
ros2/plantmate_jetrover_bridge/robot_controller/robot_heartbeat_node.py
ros2/plantmate_jetrover_bridge/robot_controller/run_drive_demo.py
ros2/plantmate_jetrover_bridge/robot_controller/run_move_demo.py
ros2/plantmate_jetrover_bridge/robot_controller/run_turn_demo.py
ros2/plantmate_jetrover_bridge/robot_controller/run_water_sequence.py
ros2/plantmate_jetrover_bridge/robot_controller/run_watering_end_demo.py
server/mqtt_device_registry.c
server/server
SESSION_HANDOFF_2026-06-01.md
```

Do not accidentally include the broad Android UI/resource changes unless that
separate UI work is intentionally being committed.

## Verification Performed

Local syntax/config checks:

```bash
python3 -m py_compile \
  ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py \
  ros2/plantmate_jetrover_bridge/robot_controller/robot_heartbeat_node.py \
  ros2/plantmate_jetrover_bridge/robot_controller/run_move_demo.py \
  ros2/plantmate_jetrover_bridge/robot_controller/run_water_sequence.py \
  ros2/plantmate_jetrover_bridge/robot_controller/run_watering_end_demo.py \
  ros2/plantmate_jetrover_bridge/robot_controller/run_turn_demo.py \
  ros2/plantmate_jetrover_bridge/robot_controller/run_drive_demo.py
```

```bash
python3 -m json.tool ros2/plantmate_jetrover_bridge/robot_controller/robot_config.json
```

Server rebuild:

```bash
cd server
make
```

Android compile was not verified because `android/local.properties` points to a
Windows SDK path that is not available from WSL:

```text
SDK location not found
```

## Recommended Next Owner Steps

1. Deploy the committed ROS2 files to JetRover.
2. Start controller, heartbeat, and command node.
3. Test `run_turn_demo.py --yaw 0.5` and `run_turn_demo.py --yaw -0.5` manually.
4. If one direction is wrong or no progress is detected, add direction sign config.
5. Test `run_drive_demo.py --distance 0.5` and `--distance -0.5`.
6. Only after those pass, test full `run_water_sequence.py --x 1.0 --y -1.0`.
7. If final position still drifts, stop tuning odom-only control and add marker or docking correction.
