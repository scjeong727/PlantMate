# ROS2 and MQTT Integration Notes

Date: 2026-05-19

> Deprecated: 이 문서는 초기 통합 과정의 작업 노트입니다. 최신 실행 절차와 설정값은
> `docs/plantmate_ros2_current_setup.md`를 기준으로 확인하세요.

## Current Direction

The server currently routes robot-related commands through MQTT bindings first,
but publishes the command using the ROS bridge topic and payload shape.
The intended short-term flow is:

```text
Client/App -> Server -> MQTT /plantmate/robot_command -> Jetrover ROS bridge -> ROS2
```

The MQTT routing table is being kept for now to decide whether a plant has a
robot or water binding. Once a binding exists, the server publishes the command
to the bridge topic instead of the old device-specific command topic.

## Implemented Server Changes

- Added `server/ros2_bridge.c` and `server/ros2_bridge.h`.
- Added `ROBOT_COMMAND` handling in `server/request_thread.c`.
- Added ROS2 fallback for watering when no serial or MQTT water device is available.
- Added MQTT-first routing for robot commands, now using the ROS bridge topic:

```text
ROBOT_COMMAND
1. Check mqtt_device_bindings for role='robot'
2. If found, publish to MQTT topic /plantmate/robot_command
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

Current MQTT bridge topic:

```text
/plantmate/robot_command
```

Current bridge payload shape:

```json
{"plantId":5,"action":"move","detail":"linear=0.2 angular=0.0"}
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

Example for current ROS bridge commands:

```bash
python3 fake_ros2_mqtt_client.py --topic /plantmate/robot_command --show-topic
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

Previous device-specific MQTT topic:

```text
device/water/jetrover-1/water/command
```

Current ROS bridge MQTT topic:

```text
/plantmate/robot_command
```

Current payload:

```json
{"plantId":5,"action":"water","detail":"duration=3"}
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

Previous device-specific MQTT topic:

```text
device/jetrover/jetrover-1/move/command
```

Current ROS bridge MQTT topic:

```text
/plantmate/robot_command
```

Current payload:

```json
{"plantId":5,"action":"move","detail":"linear=0.2 angular=0.0"}
```

This was verified with:

```bash
python3 fake_ros2_mqtt_client.py --topic /plantmate/robot_command --show-topic
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
python3 fake_ros2_mqtt_client.py --host 127.0.0.1 --topic /plantmate/robot_command --show-topic
```

Previously observed output before switching to the bridge topic:

```text
listening 127.0.0.1:1883 topics=device/jetrover/jetrover-1/move/command
device/jetrover/jetrover-1/move/command {"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
```

Expected output after the bridge-topic alignment:

```text
listening 127.0.0.1:1883 topics=/plantmate/robot_command
/plantmate/robot_command {"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
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
python3 fake_ros2_mqtt_client.py --host 127.0.0.1 --topic /plantmate/robot_command --show-topic
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

## Current Verified State After Bridge Alignment

The server has been aligned to the current ROS bridge node topic format.

Current command topic:

```text
/plantmate/robot_command
```

Current command payload:

```json
{"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
```

Water command payload:

```json
{"plantId":5,"action":"water","detail":"duration=3"}
```

Verified test commands:

Terminal 1:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
make -B
./server
```

Terminal 2:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
python3 fake_ros2_mqtt_client.py --host 127.0.0.1 --topic /plantmate/robot_command --show-topic
```

Terminal 3, move:

```bash
{
  printf 'LOGIN 1 1\n'
  sleep 0.3
  printf 'ROBOT_COMMAND 5 move linear=0.200 angular=0.000\n'
  sleep 0.5
} | nc -w 2 127.0.0.1 9000
```

Observed fake client output:

```text
/plantmate/robot_command {"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
```

Terminal 3, water:

```bash
{
  printf 'LOGIN 1 1\n'
  sleep 0.3
  printf 'WATER_PLANT 5 3\n'
  sleep 0.8
} | nc -w 3 127.0.0.1 9000
```

Observed fake client output:

```text
/plantmate/robot_command {"plantId":5,"action":"water","detail":"duration=3"}
```

Important implementation note:

- `server/mqtt_adapter_publish_bridge_command()` publishes MQTT messages to the
  ROS bridge topic.
- `ROBOT_COMMAND` now uses the bridge topic when a `role='robot'` binding exists.
- `WATER_PLANT` now uses the bridge topic when a `role='water'` binding exists.
- Android TCP mode still connects to server port `9000`.
- Android MQTT mode can connect to server port `1883`, but it talks to the
  server RPC topics first; it does not bypass the server to command ROS directly.

## Restored ROS Bridge Node

The deleted ROS bridge node files were restored from commit
`a83210d534c513bef88884ff7a190bb509bbce04`:

```text
ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py
ros2/plantmate_jetrover_bridge/robot_controller/사용방법.txt
```

Current node behavior:

- Connects to an MQTT broker.
- Subscribes to `/plantmate/robot_command`.
- Also subscribes to ROS2 topic `/plantmate/robot_command`.
- Parses JSON payload fields:
  - `action`
  - `plantId`
  - `detail`
- Currently accepts only `action='water'`.
- Publishes ACK status to:

```text
device/arm/robot-1/status
```

Current limitation:

- The node does not yet process `move`.
- The node does not yet publish `geometry_msgs/msg/Twist` to `/cmd_vel`.
- The node does not yet execute the restored JetRover action group files.
- The current local WSL environment cannot run it because ROS2, `rclpy`,
  `colcon`, and `paho-mqtt` are not installed.

## Database State

Current database:

```text
plant_db
```

Current tables:

```text
events
mqtt_device_bindings
mqtt_live_devices
plants
sensor_data
users
watering_log
```

Current `plants` table has no location columns. Existing columns are:

```text
plant_id
user_id
name
type
created_at
temp_min
temp_max
humi_min
humi_max
soil_min
soil_max
light_min
light_max
```

Current `plant_id=5` MQTT bindings:

```text
plant_id  role   device_type  device_id
5         water  water        jetrover-1
5         robot  jetrover     jetrover-1
```

Database backup created:

```text
server/plant_db_backup_20260519_154201.sql
```

This backup file is useful for local transfer or restore, but should normally
not be committed unless the team explicitly wants database snapshots in git.

## Recommended Next Work

1. Add plant location columns to the DB.

Recommended schema:

```sql
ALTER TABLE plants
ADD COLUMN location_x DOUBLE NOT NULL DEFAULT 0,
ADD COLUMN location_y DOUBLE NOT NULL DEFAULT 0,
ADD COLUMN location_z DOUBLE NOT NULL DEFAULT 0,
ADD COLUMN location_label VARCHAR(64) NOT NULL DEFAULT '';
```

Example data:

```sql
UPDATE plants
SET location_x = 1.2,
    location_y = 0.8,
    location_z = 0.0,
    location_label = 'rack-A-3'
WHERE plant_id = 5;
```

2. Extend the server bridge payload with location data.

Recommended payload:

```json
{
  "plantId": 5,
  "action": "move",
  "detail": "linear=0.200 angular=0.000",
  "location": {
    "x": 1.2,
    "y": 0.8,
    "z": 0.0,
    "label": "rack-A-3"
  }
}
```

Recommended rule:

- Android sends only `plantId` and command intent.
- Server uses `plantId` to look up location from DB.
- Server attaches location to the bridge payload.
- ROS bridge trusts the server payload and moves/acts accordingly.

3. Update Android for Jetrover selection.

Needed UI:

```text
식물 선택
Jetrover 선택
로봇 연결
```

Needed binding:

```text
plant_id=<selected plant>
role=robot
device_type=jetrover
device_id=<selected Jetrover>
```

Water can either use:

```text
role=water, device_type=water, device_id=jetrover-1
```

or be unified later as:

```text
role=water, device_type=jetrover, device_id=jetrover-1
```

4. Update the ROS bridge node.

Minimum next ROS bridge changes:

- Accept `move`.
- Parse `detail` values such as:

```text
linear=0.200 angular=0.000
```

- Publish to:

```text
/cmd_vel
```

using:

```text
geometry_msgs/msg/Twist
```

- Keep `water` action support.
- Later connect `water` to JetRover action group files:

```text
ros2/plantmate_jetrover_bridge/action_groups/watering_demo.d6a
ros2/plantmate_jetrover_bridge/action_groups/watering_end_demo.d6a
```

5. Commit pending code intentionally.

Current pending changes include:

- Server bridge topic alignment.
- Documentation updates.
- Restored `robot_controller` files.
- Rebuilt `server/server` binary.
- Local DB backup SQL file.

Recommended commit policy:

- Commit source/docs/restored ROS files.
- Do not commit `server/server` binary unless the project intentionally tracks
  compiled server binaries.
- Do not commit `server/plant_db_backup_20260519_154201.sql` unless a database
  snapshot is explicitly required.

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

## Next-Day Handoff Summary

Date prepared: 2026-05-19

### Repository State

Use the WSL Git metadata directory when running Git commands from this workspace:

```bash
git --git-dir=/mnt/c/Users/user/DeploymentProgram/.git-wsl --work-tree=/mnt/c/Users/user/DeploymentProgram status --short --branch
```

Current branch:

```text
develop
```

Current pushed state:

```text
87594d5 (HEAD -> develop, origin/develop) Update ROS bridge test command docs
57d0b47 Align server commands with ROS bridge topic
76d8a42 update mysql db
```

The `develop` branch has been pushed to:

```text
https://github.com/scjeong727/PlantMate.git
```

Current remaining local-only file:

```text
server/plant_db_backup_20260519_154201.sql
```

This file is an extra timestamped DB backup and is not committed. The tracked DB
backup file from commit `76d8a42` is:

```text
server/plant_db_backup.sql
```

If the DB file from `76d8a42` needs to be restored again:

```bash
git --git-dir=/mnt/c/Users/user/DeploymentProgram/.git-wsl --work-tree=/mnt/c/Users/user/DeploymentProgram restore --source 76d8a42 -- server/plant_db_backup.sql
```

### Current Working Architecture

The current verified command path is:

```text
Android App or TCP client
-> Server
-> MQTT broker
-> /plantmate/robot_command
-> fake_ros2_mqtt_client or future Jetrover ROS bridge
```

Current command topic:

```text
/plantmate/robot_command
```

Move payload example:

```json
{"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
```

Water payload example:

```json
{"plantId":5,"action":"water","detail":"duration=3"}
```

### Files Changed In This Work

Important server files:

```text
server/request_thread.c
server/mqtt_adapter.c
server/mqtt_adapter.h
server/ros2_bridge.c
server/ros2_bridge.h
server/fake_ros2_mqtt_client.py
```

Important Android files:

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

Important ROS2 files:

```text
ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py
ros2/plantmate_jetrover_bridge/robot_controller/사용방법.txt
ros2/plantmate_jetrover_bridge/action_groups/watering_demo.d6a
ros2/plantmate_jetrover_bridge/action_groups/watering_end_demo.d6a
```

### Quick Manual Test

Terminal 1:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
make -B
./server
```

Terminal 2:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
python3 fake_ros2_mqtt_client.py --host 127.0.0.1 --topic /plantmate/robot_command --show-topic
```

Terminal 3, move:

```bash
{
  printf 'LOGIN 1 1\n'
  sleep 0.3
  printf 'ROBOT_COMMAND 5 move linear=0.200 angular=0.000\n'
  sleep 0.5
} | nc -w 2 127.0.0.1 9000
```

Expected fake client output:

```text
/plantmate/robot_command {"plantId":5,"action":"move","detail":"linear=0.200 angular=0.000"}
```

Terminal 3, water:

```bash
{
  printf 'LOGIN 1 1\n'
  sleep 0.3
  printf 'WATER_PLANT 5 3\n'
  sleep 0.8
} | nc -w 3 127.0.0.1 9000
```

Expected fake client output:

```text
/plantmate/robot_command {"plantId":5,"action":"water","detail":"duration=3"}
```

### Android Test Reminder

Open this project in Android Studio:

```text
C:\Users\user\DeploymentProgram\android
```

Run setup:

```text
1. Sync Gradle.
2. Start server and fake_ros2_mqtt_client.
3. Run the Android app.
4. Login with TCP/IP mode.
5. Use host 10.0.2.2 for emulator, or the server PC LAN IP for a physical phone.
6. Use port 9000.
7. Open the 로봇 tab and press 이동.
```

Current Android Gradle Plugin version:

```toml
agp = "9.1.0"
```

### Database Reminder

Current database name:

```text
plant_db
```

Current `plant_id=5` bindings:

```text
plant_id  role   device_type  device_id
5         water  water        jetrover-1
5         robot  jetrover     jetrover-1
```

Check bindings:

```bash
mysql -uroot -p1234 plant_db -e "SELECT plant_id, role, device_type, device_id FROM mqtt_device_bindings WHERE plant_id=5;"
```

After changing bindings, restart `./server` because bindings are preloaded on
server startup.

### Recommended Next Work For Tomorrow

1. Add plant location columns to the `plants` table:

```sql
ALTER TABLE plants
ADD COLUMN location_x DOUBLE NOT NULL DEFAULT 0,
ADD COLUMN location_y DOUBLE NOT NULL DEFAULT 0,
ADD COLUMN location_z DOUBLE NOT NULL DEFAULT 0,
ADD COLUMN location_label VARCHAR(64) NOT NULL DEFAULT '';
```

2. Update the server payload builder so bridge commands include location data
looked up by `plantId`.

3. Update Android robot UI so the user can select:

```text
식물 선택
Jetrover 선택
로봇 연결
```

4. Update the ROS bridge node:

- Accept `move`.
- Parse `linear` and `angular` from `detail`.
- Publish `geometry_msgs/msg/Twist` to `/cmd_vel`.
- Keep `water` support.
- Later connect `water` to the restored JetRover action group files.

5. Decide whether to keep or delete the local-only backup:

```text
server/plant_db_backup_20260519_154201.sql
```
