# PlantMate Session Handoff - 2026-05-29

## Workspace

Local root:

```text
/mnt/c/Users/user/DeploymentProgram
```

Main folders:

```text
android/
server/
ros2/
docs/
```

Current git branch:

```text
develop
```

Current HEAD:

```text
a8ae4f1799bd115715f2b6fb9989453f339e24b2
Add robot_move, sequence
```

The local branch is ahead of `origin/develop` by this commit.

## Current Working Tree Notes

There are many pre-existing Android UI/resource changes in the working tree.
Do not reset or discard them unless explicitly requested.

Important local edits from this session:

```text
ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py
android/app/src/main/java/kr/ac/dju/plantmate/protocol/mqtt/MqttPlantGateway.java
server/server
```

`server/server` is a rebuilt binary from `make -B`.

## Commit/Version Policy

The desired code baseline is:

```text
Server and ROS2 bridge scripts:
a8ae4f1799bd115715f2b6fb9989453f339e24b2

PING/PONG responsibility split:
d882c6a8213327147e9a174651c7f1ac9cedf1f2
```

Meaning:

```text
robot_command_node.py
- handles move/water
- ignores ping
- does not publish PONG

robot_heartbeat_node.py
- handles ping
- publishes PONG
```

## ROS2 Files To Deploy To JetRover

Target folder on JetRover:

```text
/home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
```

Overwrite these files:

```text
robot_command_node.py
robot_heartbeat_node.py
robot_config.json
```

Add these files if missing:

```text
run_move_demo.py
run_water_sequence.py
pick_demo.py
run_watering_demo.py
run_watering_end_demo.py
```

The JetRover currently had only:

```text
PlantMate
__init__.py
__pycache__
robot_command_node.py
robot_config.json
robot_heartbeat_node.py
사용방법.txt
```

After copying, run on JetRover:

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
chmod +x *.py
```

## ROS2 Runtime

Start heartbeat:

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_heartbeat_node.py
```

Expected heartbeat logs:

```text
Heartbeat online. Subscribed: /plantmate/robot_command | Pub: device/arm/robot-1/status
PONG sent: {'eventType': 'PONG', 'deviceType': 'arm', 'deviceId': 'robot-1', 'requestId': 'ping-...'}
```

Start command node in another terminal:

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_command_node.py
```

Expected command node behavior:

```text
MQTT connected! Subscribed to /plantmate/robot_command | Pub: device/arm/robot-1/status
move/water commands are processed
ping is ignored by command node
PONG is emitted only by robot_heartbeat_node.py
```

If command node logs `Unsupported action: ping`, the JetRover does not have the latest `robot_command_node.py`.

## ROS2 Config

Important `robot_config.json` values:

```json
{
  "mqtt_host": "192.168.0.6",
  "mqtt_port": 1883,
  "device_type": "arm",
  "device_id": "robot-1",
  "command_topic": "/plantmate/robot_command",
  "ros_command_topic": "/plantmate/robot_command",
  "cmd_vel_topic": "/controller/cmd_vel"
}
```

## Server State

Server binary was rebuilt with:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
make -B
```

Server was observed running:

```text
./server pid=8314
1883 MQTT   LISTEN
9000 TCP    LISTEN
9001 sensor LISTEN
```

If `mqtt bind: Address already in use` appears, port `1883` is already occupied.
Earlier this was caused by Mosquitto:

```text
/usr/sbin/mosquitto -c /etc/mosquitto/mosquitto.conf
```

The user wanted Mosquitto removed. Removal requires sudo from a real terminal:

```bash
sudo systemctl stop mosquitto
sudo systemctl disable mosquitto
sudo apt-get purge -y mosquitto mosquitto-clients libmosquitto1
sudo apt-get autoremove -y --purge
```

## Database State

Database:

```text
plant_db
```

Connection used:

```bash
mysql -uroot -p1234 plant_db
```

Schema matches the current server code:

```text
plants.position_x
plants.position_y
mqtt_device_bindings
mqtt_live_devices
uq_plant_role
uq_device
```

Important current plant names:

```text
plant_id=5 -> name=1
plant_id=8 -> name=화이트스타
```

Current binding status:

```text
plant_id  role   device_type  device_id
5         robot  jetrover     jetrover-1
5         water  water        jetrover-1
8         robot  arm          robot-1
8         water  arm          robot-1
```

Use `plant_id=8` for ROS2/PING-PONG tests.

Expected live device row when heartbeat works:

```text
device_type=device arm
device_id=robot-1
online=1
status_payload={"eventType":"PONG",...}
```

Actual earlier state before JetRover heartbeat:

```text
arm / robot-1 / online=0
```

Robot command offline check was verified:

```text
LOGIN 1 1
ROBOT_COMMAND 8 move x=1.0 y=0.0
-> ERROR robot_offline
```

This is expected when heartbeat is not active or server has not received recent PONG.

## Android State

Project path for Android Studio:

```text
C:\Users\user\DeploymentProgram\android
```

Android Gradle Plugin is intentionally kept at:

```toml
agp = "9.2.1"
```

Gradle wrapper:

```text
Gradle 9.4.1
```

The installed Android Studio reported:

```text
The project is using an incompatible version (AGP 9.2.1).
Latest supported version is AGP 9.1.0.
```

Decision:

```text
Do not downgrade project to AGP 9.1.0.
Update Android Studio instead.
```

Available update shown by user:

```text
Android Studio Panda 4 | 2025.3.4 Patch 1
```

## Android MQTT Binding Policy

TCP mode is currently ignored for binding work.
The target behavior is MQTT-only.

`MqttPlantGateway.java` was updated so Android MQTT binding uses ROS2 device type:

```text
ROS2_DEVICE_TYPE = "arm"
```

Now Android MQTT mode should request/bind:

```text
robot binding:
role=robot, deviceType=arm, deviceId=robot-1

water binding:
role=water, deviceType=arm, deviceId=robot-1
```

This lets Android UI connect a plant to the ROS2 robot if:

```text
1. ./server is running
2. robot_heartbeat_node.py is running
3. mqtt_live_devices has arm/robot-1 online=1
4. Android connects in MQTT mode
```

In Android UI:

```text
Plant detail screen
-> robot refresh
-> select robot-1
-> bind/connect robot
```

## Verification Limitations

Android Java compile was attempted:

```bash
cd android
./gradlew :app:compileDebugJavaWithJavac
```

It failed because the local SDK path is invalid:

```text
SDK location not found.
android/local.properties sdk.dir path does not exist.
```

Use Android Studio after updating it, or fix `android/local.properties`.

## Useful Checks

DB checks:

```bash
mysql -uroot -p1234 plant_db -e "SELECT plant_id, role, device_type, device_id FROM mqtt_device_bindings ORDER BY plant_id, role;"
mysql -uroot -p1234 plant_db -e "SELECT device_type, device_id, online, updated_at, status_payload FROM mqtt_live_devices;"
```

Port checks:

```bash
ss -ltnp | grep -E ':1883|:9000|:9001'
```

Server test:

```bash
printf 'LOGIN 1 1\n' | nc -w 2 127.0.0.1 9000
```

Expected:

```text
OK {"user_id":5}
```

## 2026-05-29 Heartbeat Reconnect Debug Notes

Goal for this pass:

```text
Keep the latest commit payload/topic direction, but split runtime responsibility:

robot_command_node.py
- handles move/water commands
- does not handle PING/PONG

robot_heartbeat_node.py
- handles PING
- publishes PONG

Server must keep running when heartbeat is stopped and started again.
```

Important topic/payload baseline from commit:

```text
a8ae4f1799bd115715f2b6fb9989453f339e24b2
```

Android MQTT `robotCommand` path publishes command payloads to device-specific topics:

```text
device/arm/robot-1/move/command
device/arm/robot-1/water/command
```

Payload shape:

```json
{"plantId":8,"action":"move","detail":"x=1.0 y=0.0"}
```

Water payload detail may be JSON encoded as a string:

```json
{"plantId":8,"action":"water","detail":"{\"duration\":5,\"x\":5.000,\"y\":4.000}"}
```

PING/PONG still uses the bridge command/status topics:

```text
Server PING topic: /plantmate/robot_command
Heartbeat PONG topic: device/arm/robot-1/status
```

Current ROS node responsibility target:

```text
robot_heartbeat_node.py
- subscribes /plantmate/robot_command
- receives action=ping
- publishes PONG to device/arm/robot-1/status

robot_command_node.py
- subscribes /plantmate/robot_command
- subscribes device/arm/robot-1/move/command
- subscribes device/arm/robot-1/water/command
- ignores action=ping
- handles action=move/action=water
```

MQTT client IDs were made explicit to distinguish reconnecting nodes in server logs:

```text
heartbeat-arm-robot-1
command-arm-robot-1
```

No direct Python socket communication is used in ROS nodes. They still use:

```python
paho.mqtt.client
```

Observed reconnect bug:

```text
1. Start ./server.
2. Start robot_heartbeat_node.py.
3. Heartbeat connects and sends PONG normally.
4. Stop heartbeat with Ctrl+C.
5. Server logs DISCONNECT and close.
6. Start heartbeat again.
7. Server no longer logs accept/CONNECT.
```

Important server logs from the first, working heartbeat run:

```text
[mqtt] accept fd=10 slot=1
[mqtt] CONNECT fd=10 client_id=heartbeat-arm-robot-1 body_len=33
[mqtt] SUBSCRIBE client_id=heartbeat-arm-robot-1 topic=/plantmate/robot_command qos=1
[mqtt] PUBLISH client_id=heartbeat-arm-robot-1 topic=device/arm/robot-1/status qos=1 payload=...
[mqtt] DISCONNECT client_id=heartbeat-arm-robot-1
[mqtt] close fd=10 client_id=heartbeat-arm-robot-1
```

Root cause found:

```text
server/mqtt_adapter.c had an extra pthread_mutex_lock(&g_clients_mutex)
after handling MQTT DISCONNECT packet type 14.
```

Bad flow before fix:

```c
pthread_mutex_lock(&g_clients_mutex);
mqtt_close_client(client);
pthread_mutex_unlock(&g_clients_mutex);
pthread_mutex_lock(&g_clients_mutex);
break;
```

The last lock had no matching unlock before breaking out of the switch path,
which could deadlock the MQTT adapter loop after a client disconnect.

Fix:

```c
pthread_mutex_lock(&g_clients_mutex);
mqtt_close_client(client);
pthread_mutex_unlock(&g_clients_mutex);
break;
```

This does not remove synchronization around `mqtt_close_client`; it only removes
the unmatched second lock.

Additional server diagnostics added temporarily in:

```text
server/mqtt_adapter.c
```

Diagnostic log examples:

```text
[mqtt] accept fd=...
[mqtt] CONNECT fd=... client_id=...
[mqtt] SUBSCRIBE client_id=... topic=...
[mqtt] PUBLISH client_id=... topic=... qos=...
[mqtt] PINGREQ client_id=...
[mqtt] DISCONNECT client_id=...
[mqtt] recv failed fd=... client_id=...
[mqtt] close fd=... client_id=...
```

Server was rebuilt successfully:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
make -B
```

After applying this server binary, retest:

```text
1. Start ./server.
2. Start robot_heartbeat_node.py.
3. Confirm PONG.
4. Stop heartbeat with Ctrl+C.
5. Do not restart server.
6. Start robot_heartbeat_node.py again.
7. Confirm server logs a new accept/CONNECT/SUBSCRIBE/PONG sequence.
```

Next planned test:

```text
With server and heartbeat connected, move JetRover outside Wi-Fi range.
Confirm MQTT disconnect/loss is detected, mqtt_live_devices becomes offline,
and move/water commands are rejected or cancelled instead of continuing blindly.
```

Things to observe during Wi-Fi range test:

```text
JetRover heartbeat log:
- MQTT disconnected unexpectedly: rc=...

Server DB:
- mqtt_live_devices online changes from 1 to 0 after timeout

Server command result:
- ROBOT_COMMAND / Android robotCommand should return robot_offline

Robot behavior:
- If a move/water sequence is already running when Wi-Fi is lost, verify whether
  current scripts stop safely or keep running locally. This may require a
  separate cancellation/stop mechanism if not already implemented.
```
