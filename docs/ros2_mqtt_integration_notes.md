# ROS2 and MQTT Integration Notes

Date: 2026-05-19

## Current Direction

The server currently routes robot-related commands through MQTT bindings first.
The intended short-term flow is:

```text
Client/App -> Server -> MQTT -> fake_ros2_mqtt_client or Jetrover gateway -> ROS2
```

The MQTT routing table is being kept for now. Later, the fake client can be replaced
with a real Jetrover-side MQTT-to-ROS2 gateway.

## Implemented Server Changes

- Added `server/ros2_bridge.c` and `server/ros2_bridge.h`.
- Added `ROBOT_COMMAND` handling in `server/request_thread.c`.
- Added ROS2 fallback for watering when no serial or MQTT water device is available.
- Added MQTT-first routing for robot commands:

```text
ROBOT_COMMAND
1. Check mqtt_device_bindings for role='robot'
2. If found, publish to MQTT
3. If not found, fallback to ROS2 bridge
```

The command format is:

```text
ROBOT_COMMAND plant_id action [detail]
```

Example:

```text
ROBOT_COMMAND 5 move linear=0.2 angular=0.0
```

## Fake MQTT Client

Added:

```text
server/fake_ros2_mqtt_client.py
```

Purpose:

- Connect to the server MQTT broker at `127.0.0.1:1883`.
- Subscribe to device command topics.
- Print received payloads as one line.
- Optionally print the topic with `--show-topic`.
- Supports comma-separated actions or repeated `--topic`.

Example for movement commands:

```bash
python3 fake_ros2_mqtt_client.py --device-type jetrover --device-id jetrover-1 --action move --show-topic
```

Example for water commands:

```bash
python3 fake_ros2_mqtt_client.py --device-type water --device-id jetrover-1 --action water --show-topic
```

## Current MQTT Bindings

For `plant_id=5`, these bindings were inserted:

```text
plant_id  role   device_type  device_id
5         robot  jetrover     jetrover-1
5         water  water        jetrover-1
```

Check with:

```bash
mysql -uroot -p1234 plant_db -e "SELECT plant_id, role, device_type, device_id FROM mqtt_device_bindings WHERE plant_id=5;"
```

Insert or update the robot binding:

```bash
mysql -uroot -p1234 plant_db -e "INSERT INTO mqtt_device_bindings (plant_id, role, device_type, device_id) VALUES (5, 'robot', 'jetrover', 'jetrover-1') ON DUPLICATE KEY UPDATE device_type='jetrover', device_id='jetrover-1';"
```

Insert or update the water binding:

```bash
mysql -uroot -p1234 plant_db -e "INSERT INTO mqtt_device_bindings (plant_id, role, device_type, device_id) VALUES (5, 'water', 'water', 'jetrover-1') ON DUPLICATE KEY UPDATE device_type='water', device_id='jetrover-1';"
```

Important: restart `./server` after changing bindings, because bindings are
preloaded at server startup.

## Verified Water Command

Input through server TCP port:

```text
LOGIN 1 1
WATER_PLANT 5 3
```

Expected MQTT topic:

```text
device/water/jetrover-1/water/command
```

Expected payload:

```json
{"plantId":5,"duration":3}
```

This was verified with the fake MQTT client.

## Verified Move Command

Input through server TCP port:

```text
LOGIN 1 1
ROBOT_COMMAND 5 move linear=0.2 angular=0.0
```

Server response:

```text
OK {"message":"robot_command_published"}
```

Expected MQTT topic:

```text
device/jetrover/jetrover-1/move/command
```

Expected payload:

```json
{"plantId":5,"action":"move","detail":"linear=0.2 angular=0.0"}
```

This was verified with:

```bash
python3 fake_ros2_mqtt_client.py --device-type jetrover --device-id jetrover-1 --action move --show-topic
```

## Android App Robot Command Path

Added an Android robot control path so Android Studio can test the same server
route that was previously tested with `nc`.

Changed Android files:

```text
android/app/src/main/java/kr/ac/dju/plantmate/ui/RobotFragment.java
android/app/src/main/res/layout/fragment_robot.xml
android/app/src/main/res/menu/menu_bottom_navigation.xml
android/app/src/main/java/kr/ac/dju/plantmate/MainActivity.java
android/app/src/main/java/kr/ac/dju/plantmate/protocol/PlantGateway.java
android/app/src/main/java/kr/ac/dju/plantmate/protocol/tcp/TcpPlantGateway.java
android/app/src/main/java/kr/ac/dju/plantmate/protocol/mqtt/MqttPlantGateway.java
android/app/src/main/java/kr/ac/dju/plantmate/repository/PlantRepository.java
android/app/src/main/java/kr/ac/dju/plantmate/service/PlantClientService.java
```

The Android app now has a `로봇` bottom navigation tab. It lets the user select a
plant and send a move command with `linear` and `angular` values.

TCP mode sends this server command:

```text
ROBOT_COMMAND 5 move linear=0.200 angular=0.000
```

MQTT app mode sends an RPC action named `robotCommand`; the server handles it
with the same MQTT-first robot routing logic.

Server-side MQTT RPC support was added in:

```text
server/mqtt_adapter.c
```

The server-side RPC request shape is:

```json
{"action":"robotCommand","plantId":5,"robotAction":"move","detail":"linear=0.200 angular=0.000"}
```

## Verified Android to Fake ROS2 Command

The Android app robot tab was used to send a move command through the server.

Fake ROS2/Jetrover MQTT client command:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
python3 fake_ros2_mqtt_client.py --host 127.0.0.1 --device-type jetrover --device-id jetrover-1 --action move --show-topic
```

Observed output:

```text
listening 127.0.0.1:1883 topics=device/jetrover/jetrover-1/move/command
device/jetrover/jetrover-1/move/command {"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
```

This verifies:

```text
Android App -> Server -> MQTT broker -> fake_ros2_mqtt_client
```

## Android Studio Compatibility Note

The project was using Android Gradle Plugin `9.2.1`, but the local Android
Studio installation reported support up to `9.1.0`. The AGP version was changed
to:

```text
android/gradle/libs.versions.toml
agp = "9.1.0"
```

If Gradle sync next fails on SDK availability, install the required Android API
36 SDK components from Android Studio SDK Manager or lower `compileSdk` to a
locally installed SDK version.

## Manual Test Sequence

Terminal 1:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
make
./server
```

Terminal 2:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
python3 fake_ros2_mqtt_client.py --device-type jetrover --device-id jetrover-1 --action move --show-topic
```

Terminal 3:

```bash
nc 127.0.0.1 9000
```

Then type:

```text
LOGIN 1 1
ROBOT_COMMAND 5 move linear=0.2 angular=0.0
```

Android Studio sequence:

```text
1. Open C:\Users\user\DeploymentProgram\android in Android Studio.
2. Sync Gradle.
3. Start the server and fake_ros2_mqtt_client.
4. Run the Android app.
5. Login with TCP/IP mode, host 10.0.2.2 for emulator or the server PC LAN IP
   for a physical phone, port 9000.
6. Open the 로봇 tab and press 이동.
```

## ROS2 Status

The repository has a ROS2 package scaffold:

```text
ros2/plantmate_jetrover_bridge
```

But the current environment did not have ROS2 available:

- `ros2` command was not found.
- `colcon` was not found.
- `python3 -c "import rclpy"` failed.

So the current verified path is MQTT-based. Real ROS2 integration still needs a
Jetrover-side node or gateway that receives MQTT commands and publishes ROS2
commands such as `/cmd_vel`.

## Git History

Pushed commits on `develop`:

```text
a5f9e7b Add ROS2 and MQTT robot command test path
2097db8 Route robot commands through MQTT bindings
```

## Current Decision

Keep the MQTT routing table for now.

Reason:

- It lets the server route plant-specific robot and water commands to a named
  Jetrover device.
- It allows the fake MQTT client to be replaced later by a real Jetrover gateway.
- It avoids requiring the server machine itself to have ROS2 installed immediately.
