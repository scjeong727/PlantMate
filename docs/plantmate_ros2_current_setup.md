# PlantMate ROS2 연동 현재 정리

Date: 2026-05-21

작성 기준: Android 앱, C 서버, ROS2 JetRover 장비를 MQTT로 연결한 현재 상태.

## 전체 흐름

```text
Android 앱
-> C 서버
-> 서버 내장 MQTT broker :1883
-> /plantmate/robot_command
-> robot_command_node.py
-> /goal_pose 발행
-> /odom 기준 단순 제어
-> /controller/cmd_vel 발행
-> 로봇 controller
```

물주기는 이동과 분리하지 않고 하나로 처리한다.

```text
앱에서 물주기 클릭
-> 서버가 plant_id로 식물 x/y 좌표 조회
-> action=water, detail={"duration":5,"x":5.000,"y":4.000}
-> ROS2 노드가 해당 좌표로 이동 제어
-> 목표 도착 시 물주기 실행 로그
```

## 서버 실행

서버 위치:

```bash
cd /mnt/c/Users/user/DeploymentProgram/server
./server
```

서버가 열어야 하는 포트:

```text
1883: MQTT
9000: Android/TCP 요청
9001: 센서 수신
```

WSL 서버를 Windows LAN IP로 노출하려면 관리자 cmd 또는 PowerShell에서 portproxy/firewall 설정이 필요하다.

예시:

```bat
netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=1883 connectaddress=172.26.34.202 connectport=1883
netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=9000 connectaddress=172.26.34.202 connectport=9000

netsh advfirewall firewall add rule name="PlantMate MQTT 1883" dir=in action=allow protocol=TCP localport=1883
netsh advfirewall firewall add rule name="PlantMate TCP 9000" dir=in action=allow protocol=TCP localport=9000
```

현재 테스트에서 Windows 서버 IP는 다음 값이었다.

```text
192.168.0.6
```

## 서버 설정파일

파일:

```text
server/server_config.conf
```

다른 PC에서 주로 수정할 줄:

```conf
db_host=127.0.0.1
db_user=root
db_password=1234
db_name=plant_db
db_port=0

request_port=9000
mqtt_port=1883
sensor_port=9001
sensing_server_ip=127.0.0.1

ros2_bridge_topic=/plantmate/robot_command
```

다른 설정파일로 서버 실행:

```bash
PLANTMATE_SERVER_CONFIG=/path/to/server_config.conf ./server
```

관련 코드:

```text
server/server_config.c
server/server_config.h
server/server_main.c
server/db.c
server/request_thread.c
server/mqtt_adapter.c
server/sensor_thread.c
server/sensing.c
server/ros2_bridge.c
```

## Android 설정파일

파일:

```text
android/app/src/main/assets/plantmate_config.json
```

내용:

```json
{
  "defaultHost": "192.168.0.6",
  "mqttPort": 1883,
  "tcpPort": 9000,
  "clientId": "PlantMate-Android"
}
```

다른 서버 PC로 바뀌면 보통 `defaultHost`만 바꾸면 된다.

관련 코드:

```text
android/app/src/main/java/kr/ac/dju/plantmate/config/AppConfig.java
android/app/src/main/java/kr/ac/dju/plantmate/LoginActivity.java
android/app/src/main/java/kr/ac/dju/plantmate/protocol/mqtt/MqttPlantGateway.java
```

## ROS2 설정파일

파일:

```text
ros2/plantmate_jetrover_bridge/robot_controller/robot_config.json
```

현재 권장 내용:

```json
{
  "mqtt_host": "192.168.0.6",
  "mqtt_port": 1883,
  "device_type": "arm",
  "device_id": "robot-1",
  "command_topic": "/plantmate/robot_command",
  "ros_command_topic": "/plantmate/robot_command",
  "nav_goal_topic": "/goal_pose",
  "odom_topic": "/odom",
  "cmd_vel_topic": "/controller/cmd_vel",
  "goal_tolerance": 0.15,
  "angle_tolerance": 0.2,
  "linear_gain": 0.35,
  "angular_gain": 1.2,
  "max_linear_speed": 0.25,
  "max_angular_speed": 0.8
}
```

중요: JetRover에서 실제로 받는 속도 토픽은 `/controller/cmd_vel`이었다. 따라서 `cmd_vel_topic`은 `/controller/cmd_vel`로 둔다.

## ROS2 실행 순서

터미널 1: 로봇 controller 실행

```bash
ros2 launch controller controller.launch.py
```

터미널 2: PlantMate MQTT 명령 노드 실행

```bash
cd /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller
python3 robot_command_node.py
```

또는 파라미터 직접 지정:

```bash
python3 robot_command_node.py --ros-args \
  -p mqtt_host:=192.168.0.6 \
  -p mqtt_port:=1883 \
  -p device_type:=arm \
  -p device_id:=robot-1
```

터미널 3: 목표 좌표 확인

```bash
ros2 topic echo /goal_pose
```

터미널 4: 실제 속도 명령 확인

```bash
ros2 topic echo /controller/cmd_vel
```

## 검증된 결과

MQTT 연결 성공 로그:

```text
MQTT connected! Subscribed to /plantmate/robot_command | Pub: device/arm/robot-1/status
```

서버 물주기 성공 로그:

```text
watering start: plant_id=8 duration=5 owner=-1
watering delegated to mqtt ros bridge: type=arm id=robot-1
watering end: plant_id=8, ok=1
```

ROS2 명령 수신 성공 로그:

```text
Received command: plant_id=8, action=water, detail={"duration":5,"x":5.000,"y":4.000}
ACK sent successfully with QoS 1
[물주기 이동 시작] plant_id=8, X: 5.0, Y: 4.0, duration=5
```

`/goal_pose` 발행 확인:

```text
frame_id: map
position:
  x: 5.0
  y: 4.0
```

`/goal_pose` 상태:

```text
Type: geometry_msgs/msg/PoseStamped
Publisher count: 1
Subscription count: 1
```

## 수동 모터 토픽 테스트

전진:

```bash
ros2 topic pub /controller/cmd_vel geometry_msgs/msg/Twist \
"{linear: {x: 0.2, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}"
```

회전:

```bash
ros2 topic pub /controller/cmd_vel geometry_msgs/msg/Twist \
"{linear: {x: 0.0, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.5}}"
```

정지:

```bash
ros2 topic pub --once /controller/cmd_vel geometry_msgs/msg/Twist \
"{linear: {x: 0.0, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}"
```

## 중요한 판단 기준

```text
/plantmate/robot_command 수신됨
=> 서버와 ROS2 MQTT 연결 성공

/goal_pose에 좌표가 뜸
=> PlantMate 좌표 전달 성공

/controller/cmd_vel에 Twist가 뜸
=> robot_command_node.py가 실제 이동 명령 생성 중

/controller/cmd_vel이 뜨는데 로봇이 안 움직임
=> 모터 enable, controller, 하드웨어 드라이버 문제
```

## 현재 구현의 한계

현재 이동 제어는 단순 제어다.

```text
/odom 현재 위치 확인
목표 방향 계산
각도 오차가 크면 회전
각도 오차가 작으면 전진
목표 근처 도착하면 정지
```

아직 없는 기능:

```text
장애물 회피
경로 계획
지도 기반 navigation
정확한 도착 후 펌프/액션 그룹 실행
```

## Git에서 특정 파일만 가져오기

특정 커밋의 `robot_command_node.py`만 가져오기:

```bash
cd /tmp
rm -rf PlantMate
git clone -b develop --single-branch https://github.com/scjeong727/PlantMate.git
cd PlantMate

git show 78290b83d26bd90e41f69a8580bd59210fb1983a:ros2/plantmate_jetrover_bridge/robot_controller/robot_command_node.py > /tmp/robot_command_node.py
```

목적지로 복사:

```bash
cp /tmp/robot_command_node.py /home/ubuntu/ros2_ws/src/plantmate_jetrover_bridge/robot_controller/
```

최신 설정파일 분리 버전은 `robot_config.json`도 같이 가져와야 한다.
