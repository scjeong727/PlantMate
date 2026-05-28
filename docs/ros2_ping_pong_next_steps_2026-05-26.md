# ROS2 PING/PONG 이어가기 노트

Date: 2026-05-22
Next workday: 2026-05-26 Tuesday

## 현재 확인된 상태

서버는 MQTT 브로커 역할을 하며 `/plantmate/robot_command`로 ROS 쪽에 PING을 보낸다. ROS 쪽에서는 `robot_heartbeat_node.py`가 PING을 받고 `device/arm/robot-1/status`로 PONG을 보낸다.

실제 JetRover에서 확인된 로그:

```text
Heartbeat online. Subscribed: /plantmate/robot_command | Pub: device/arm/robot-1/status
PONG sent: {'eventType': 'PONG', 'deviceType': 'arm', 'deviceId': 'robot-1', 'requestId': 'ping-...'}
```

거리 때문에 네트워크가 끊겼을 때 확인된 로그:

```text
MQTT disconnected unexpectedly: rc=16
```

다시 범위 안으로 돌아오면 PONG이 다시 올라오는 것도 확인했다. 즉 서버와 ROS heartbeat 간 연결 감지 자체는 동작한다.

## 현재 ROS 실행 구조

JetRover에서는 최소 3개가 같이 떠 있어야 한다.

```bash
ros2 launch controller controller.launch.py
```

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_command_node.py
```

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_heartbeat_node.py
```

역할:

```text
robot_command_node.py   move/water 처리, /controller/cmd_vel 발행
robot_heartbeat_node.py PING 수신, PONG 발행
```

주의: `robot_heartbeat_node.py`만 켜져 있으면 연결 확인은 되지만 로봇은 이동하지 않는다.

## PING/PONG 형식

서버가 보내는 PING:

```json
{
  "plantId": 0,
  "action": "ping",
  "targetDeviceType": "arm",
  "targetDeviceId": "robot-1",
  "requestId": "ping-1779432818-2"
}
```

ROS heartbeat가 보내는 PONG:

```json
{
  "eventType": "PONG",
  "deviceType": "arm",
  "deviceId": "robot-1",
  "requestId": "ping-1779432818-2"
}
```

PONG topic:

```text
device/arm/robot-1/status
```

## 중요한 불일치

코드 기준으로 불일치가 있다.

ROS heartbeat 기준:

```text
device_type=arm
device_id=robot-1
```

그런데 Android 로봇 바인딩 코드는 현재 `deviceType`을 `robot`으로 보낸다.

파일:

```text
android/app/src/main/java/kr/ac/dju/plantmate/protocol/mqtt/MqttPlantGateway.java
```

현재 코드:

```java
request.put("role", "robot");
request.put("deviceType", "robot");
request.put("deviceId", deviceId.trim());
```

서버는 `ROBOT_COMMAND` 처리 시 DB 바인딩값 기준으로 online 여부를 검사한다.

파일:

```text
server/request_thread.c
```

흐름:

```text
mqtt_device_registry_get(plant_id, "robot", &mqtt_binding)
mqtt_device_registry_is_live_device_online(mqtt_binding.device_type, mqtt_binding.device_id, 10)
```

따라서 DB 바인딩이 `robot/robot-1`인데 PONG은 `arm/robot-1`로 들어오면 서버의 online 판단이 기대와 다르게 된다.

## 더 큰 문제

`role=robot` 바인딩이 없으면 서버는 offline 체크를 하지 않고 fallback으로 ROS2 bridge publish를 시도한다.

파일:

```text
server/request_thread.c
```

현재 흐름:

```text
if role=robot binding exists:
    online check
    offline이면 ERROR robot_offline
else:
    ros2_bridge_publish_command(...)
    성공하면 OK robot_command_published
```

그래서 실제로 PONG이 멈췄는데도 `ERROR robot_offline`이 안 보일 수 있다.

가능한 원인:

```text
1. plant_id=8에 role=robot 바인딩이 없음
2. 바인딩은 있지만 device_type/device_id가 PONG의 arm/robot-1과 다름
3. 서버가 최신 PING/PONG 코드로 rebuild/restart되지 않음
```

## 화요일에 먼저 확인할 것

1. DB 바인딩 확인

```bash
mysql -uroot -p1234 plant_db -e "SELECT plant_id, role, device_type, device_id FROM mqtt_device_bindings WHERE plant_id=8;"
```

기대값:

```text
plant_id  role   device_type  device_id
8         robot  arm          robot-1
8         water  arm          robot-1
```

2. live device 확인

```bash
mysql -uroot -p1234 plant_db -e "SELECT device_type, device_id, online, updated_at, status_payload FROM mqtt_live_devices;"
```

PONG 수신 중 기대값:

```text
arm  robot-1  1  최근 시간  {"eventType":"PONG",...}
```

3. 서버 재빌드 및 재시작

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
make
./server
```

기존 서버가 떠 있으면 먼저 종료해야 한다.

4. 끊김 테스트

로봇을 멀리 보내서 `PONG sent`가 멈춘 뒤 10초 이상 기다리고 서버 PC에서:

```bash
{
  printf 'LOGIN 1 1\n'
  sleep 0.3
  printf 'ROBOT_COMMAND 8 move x=1.0 y=0.0\n'
  sleep 0.5
} | nc -w 2 127.0.0.1 9000
```

기대 응답:

```text
ERROR robot_offline
```

## 수정 방향

기준을 아래처럼 통일한다.

```text
role=robot, device_type=arm, device_id=robot-1
role=water, device_type=arm, device_id=robot-1
PONG: device_type=arm, device_id=robot-1
```

Android 수정 후보:

```java
request.put("role", "robot");
request.put("deviceType", "arm");
request.put("deviceId", deviceId.trim());
```

물주기 바인딩도 ROS 로봇이 처리하는 구조라면:

```java
request.put("role", "water");
request.put("deviceType", "arm");
request.put("deviceId", "robot-1");
```

서버 수정 후보:

```text
role=robot 바인딩이 없을 때 fallback으로 OK를 보내지 말고
PING/PONG 기반 운용 모드에서는 ERROR robot_not_bound 또는 ERROR robot_offline 반환
```

## 현재 작업트리 주의사항

현재 로컬 작업트리에는 여러 변경이 섞여 있다.

```text
1. Android UI 롤백 커밋은 로컬에 생성됨: Revert "Apply Android UI resource refresh"
2. Android UI 관련 변경/미추적 파일이 아직 작업트리에 남아 있음
3. PING/PONG 서버 코드 변경이 미커밋 상태
4. ROS heartbeat 분리 변경이 미커밋 상태
5. server/server 바이너리는 빌드 산출물로 변경되어 있을 수 있음
```

화요일에 커밋 전 반드시:

```bash
git status --short --branch
git diff --stat
```

를 보고 Android UI 잔여 변경과 PING/PONG 변경을 분리해서 처리해야 한다.

## 관련 파일

```text
server/mqtt_adapter.c
server/mqtt_device_registry.c
server/mqtt_device_registry.h
server/request_thread.c
server/watering_thread.c
ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py
ros2/plantmate_jetrover_bridge/robot_controller/robot_heartbeat_node.py
ros2/plantmate_jetrover_bridge/robot_controller/robot_config.json
android/app/src/main/java/kr/ac/dju/plantmate/protocol/mqtt/MqttPlantGateway.java
```
