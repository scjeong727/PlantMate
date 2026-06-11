import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch

fig, ax = plt.subplots(1, 1, figsize=(26, 18))
ax.set_xlim(0, 26)
ax.set_ylim(0, 18)
ax.axis('off')
fig.patch.set_facecolor('#F5F5F5')

# ──────────────── Color palette ────────────────
C_ANDROID   = '#E8F4FD'
C_ANDROID_H = '#2196F3'
C_SERVER    = '#FFF3E0'
C_SERVER_H  = '#FF6F00'
C_DB        = '#F3E5F5'
C_DB_H      = '#7B1FA2'
C_ROS       = '#E8F5E9'
C_ROS_H     = '#2E7D32'
C_IFACE     = '#FFF9C4'
C_IFACE_H   = '#F57F17'
C_BORDER    = '#BDBDBD'
C_ARROW     = '#424242'
C_TITLE_BG  = '#263238'

# ──────────────── Helper functions ────────────────
def uml_box(ax, x, y, w, h, title, attrs, methods,
            header_color, body_color, border_color='#BDBDBD',
            title_prefix='', fs=6.8, title_fs=7.5):
    """Draw a UML-style class box."""
    # Shadow
    shadow = FancyBboxPatch((x+0.07, y-0.07), w, h,
                             boxstyle="round,pad=0.05",
                             linewidth=0, facecolor='#CCCCCC', zorder=1)
    ax.add_patch(shadow)
    # Body
    body = FancyBboxPatch((x, y), w, h,
                           boxstyle="round,pad=0.05",
                           linewidth=1.2, edgecolor=border_color,
                           facecolor=body_color, zorder=2)
    ax.add_patch(body)
    # Header band height
    hh = 0.52
    header = FancyBboxPatch((x, y+h-hh), w, hh,
                             boxstyle="round,pad=0.05",
                             linewidth=0, facecolor=header_color, zorder=3)
    ax.add_patch(header)
    # Title
    if title_prefix:
        ax.text(x + w/2, y + h - hh/2 + 0.08,
                title_prefix,
                ha='center', va='center',
                fontsize=fs-0.8, color='white', fontstyle='italic', zorder=4)
        ax.text(x + w/2, y + h - hh/2 - 0.1,
                title,
                ha='center', va='center',
                fontsize=title_fs, fontweight='bold', color='white', zorder=4)
    else:
        ax.text(x + w/2, y + h - hh/2,
                title,
                ha='center', va='center',
                fontsize=title_fs, fontweight='bold', color='white', zorder=4)

    # Divider after header
    cur_y = y + h - hh
    if attrs:
        ax.plot([x+0.05, x+w-0.05], [cur_y, cur_y],
                color=border_color, lw=0.8, zorder=4)
        for a in attrs:
            cur_y -= 0.22
            ax.text(x + 0.12, cur_y + 0.05,
                    a, ha='left', va='center',
                    fontsize=fs, color='#333333',
                    fontfamily='monospace', zorder=4)

    if methods:
        ax.plot([x+0.05, x+w-0.05], [cur_y, cur_y],
                color=border_color, lw=0.8, zorder=4)
        for m in methods:
            cur_y -= 0.22
            ax.text(x + 0.12, cur_y + 0.05,
                    m, ha='left', va='center',
                    fontsize=fs, color='#1A237E',
                    fontfamily='monospace', zorder=4)

def section_bg(ax, x, y, w, h, label, color, label_color):
    """Draw a light background section with a label."""
    bg = FancyBboxPatch((x, y), w, h,
                         boxstyle="round,pad=0.1",
                         linewidth=1.5, edgecolor=label_color,
                         facecolor=color, alpha=0.35, zorder=0)
    ax.add_patch(bg)
    ax.text(x + w/2, y + h - 0.22,
            label,
            ha='center', va='center',
            fontsize=9.5, fontweight='bold', color=label_color,
            alpha=0.7, zorder=1)

def arrow(ax, x1, y1, x2, y2, label='', color='#424242', style='->', lw=1.2):
    ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle=style, color=color,
                                lw=lw, connectionstyle='arc3,rad=0.0'),
                zorder=10)
    if label:
        mx, my = (x1+x2)/2, (y1+y2)/2
        ax.text(mx, my + 0.12, label,
                ha='center', va='bottom',
                fontsize=6, color=color,
                bbox=dict(boxstyle='round,pad=0.1', fc='white', ec='none', alpha=0.8),
                zorder=11)

# ──────────────── Title ────────────────
title_bg = FancyBboxPatch((0.3, 16.8), 25.4, 0.85,
                            boxstyle="round,pad=0.1",
                            linewidth=0, facecolor=C_TITLE_BG, zorder=2)
ax.add_patch(title_bg)
ax.text(13, 17.25, 'PlantMate — System Architecture',
        ha='center', va='center',
        fontsize=16, fontweight='bold', color='white', zorder=3)

# ──────────────── Section backgrounds ────────────────
section_bg(ax,  0.3,  0.3,  5.6, 16.2, 'Android App',          C_ANDROID,  C_ANDROID_H)
section_bg(ax,  6.2,  0.3, 12.6, 16.2, 'C Server',              C_SERVER,   C_SERVER_H)
section_bg(ax, 19.1,  8.5,  6.6,  8.0, 'ROS2 (JetRover)',       C_ROS,      C_ROS_H)
section_bg(ax, 19.1,  0.3,  6.6,  7.9, 'Database',              C_DB,       C_DB_H)

# ════════════════ ANDROID boxes ════════════════
# PlantMateApp
uml_box(ax, 0.6, 13.5, 5.0, 3.0,
        'PlantMateApp', [],
        ['+login()', '+viewPlantStatus()', '+controlWatering()', '+controlRobot()'],
        C_ANDROID_H, C_ANDROID, title_prefix='<<Android>>')

# PlantClientService
uml_box(ax, 0.6, 9.8, 5.0, 3.4,
        'PlantClientService',
        ['-gateway: PlantGateway', '-worker: ExecutorService', '-userId: String'],
        ['+connect()', '+login() / signup()',
         '+loadPlants() / addPlant()',
         '+waterPlant(seconds)',
         '+robotCommand(cmd)',
         '+loadMonitorSnapshot()'],
        C_ANDROID_H, C_ANDROID)

# PlantGateway (interface)
uml_box(ax, 0.6, 7.0, 5.0, 2.5,
        'PlantGateway',
        [],
        ['+connect()', '+login() / signup()',
         '+loadPlants() / addPlant()',
         '+waterPlant()', '+robotCommand()',
         '+close()'],
        C_IFACE_H, C_IFACE, title_prefix='<<interface>>')

# TcpPlantGateway
uml_box(ax, 0.6, 4.2, 2.3, 2.5,
        'TcpPlantGateway',
        ['-client: SocketCommandClient'],
        ['+connect()', '+sendCommand()',
         '+parseResponse()'],
        C_ANDROID_H, C_ANDROID)

# MqttPlantGateway
uml_box(ax, 3.2, 4.2, 2.3, 2.5,
        'MqttPlantGateway',
        ['-mqttClient: MqttClient', '-topics: MqttTopics'],
        ['+connect()', '+publish()',
         '+subscribe()', '+onMessage()'],
        C_ANDROID_H, C_ANDROID)

# SocketCommandClient
uml_box(ax, 0.6, 0.7, 2.3, 3.2,
        'SocketCommandClient',
        ['-socket: Socket', '-host: String', '-port: int'],
        ['+connect(host, port)',
         '+sendCommand(cmd)',
         '+readResponse()',
         '+isConnected()',
         '+close()'],
        '#546E7A', '#ECEFF1')

# MqttManager
uml_box(ax, 3.2, 0.7, 2.3, 3.2,
        'MqttManager',
        ['-brokerAddress: String', '-topic: String', '-clientId: String'],
        ['+connect()',
         '+publish(topic, msg)',
         '+subscribe(topic)',
         '+disconnect()'],
        '#546E7A', '#ECEFF1')

# ════════════════ SERVER boxes ════════════════
# RequestThread
uml_box(ax, 6.5, 13.5, 3.8, 3.0,
        'RequestThread',
        ['-port: 9000 (TCP)', '-maxClients: 64'],
        ['LOGIN / ADD_USER',
         'ADD_PLANT / GET_PLANT',
         'POST_SENSOR_DATA',
         'GET_RECENT_SENSOR',
         'GET_RECENT_EVENT',
         'WATER_PLANT / ROBOT_CMD'],
        C_SERVER_H, C_SERVER)

# DBThread + Queue
uml_box(ax, 10.6, 13.5, 3.8, 3.0,
        'DBThread + DBQueue',
        ['-queue: DBQueue (cap=256)', '-db: MySQL conn'],
        ['+processRequests()',
         '+executeQuery(sql)',
         '+sendResponse(sock)',
         '+plant_repository',
         '+user_repository',
         '+sensor_repository'],
        C_SERVER_H, C_SERVER)

# SensorThread
uml_box(ax, 6.5, 10.3, 3.8, 2.9,
        'SensorThread',
        ['-port: 9001 (UDP)', '-buffer: SensorBuffer(256)'],
        ['+listenUDP()',
         '+parseSensorPacket()',
         '+fillSensorBuffer()',
         '+dispatchThresholdEvents()'],
        C_SERVER_H, C_SERVER)

# MQTT Adapter
uml_box(ax, 10.6, 10.3, 3.8, 2.9,
        'MqttAdapter',
        ['-port: 1883', '-registry: MqttDeviceRegistry'],
        ['plant/{id}/sensor',
         'plant/{id}/status',
         'device/{type}/{id}/status',
         'app/{cId}/request|response',
         '+routeToRosBridge()',
         '+updateDeviceStatus()'],
        C_SERVER_H, C_SERVER)

# WateringThread + CommandQueue
uml_box(ax, 6.5, 7.2, 3.8, 2.8,
        'WateringThread',
        ['-queue: CommandQueue (cap=100)', '-lock: WateringLock'],
        ['WaterCommand{plant_id,', '  duration, owner_sock}',
         '+executeWatering()',
         '+checkDeviceBinding()',
         '+logWateringEvent()'],
        C_SERVER_H, C_SERVER)

# ROS2Bridge (server side)
uml_box(ax, 10.6, 7.2, 3.8, 2.8,
        'ROS2Bridge (Server)',
        ['-topic: /plantmate/robot_command'],
        ['+publishCommand(plant_id,',
         '  action, detail)',
         '+waterCommand(x,y,dur)',
         '+moveCommand(x,y)'],
        C_SERVER_H, C_SERVER)

# Caches
uml_box(ax, 6.5, 4.0, 3.8, 2.9,
        'In-Memory Caches',
        ['PlantThresholdCache (256)',
         'PlantOwnerCache',
         'EventLog (128)',
         'SensorBuffer (256)'],
        ['+preload() on startup',
         '+checkThreshold(reading)',
         '+logEvent(type, msg)'],
        '#78909C', '#ECEFF1')

# SensorEventDispatcher
uml_box(ax, 10.6, 4.0, 3.8, 2.9,
        'SensorEventDispatcher',
        ['-threshCache: PlantThresholdCache'],
        ['+onSensorReading(data)',
         '+checkAllThresholds()',
         '+emitEvent(type, msg)',
         '+persistEvent(db)'],
        '#78909C', '#ECEFF1')

# server_config / cache_preload
uml_box(ax, 6.5, 0.7, 7.9, 3.0,
        'ServerConfig + CachePreload',
        ['db_host/user/pass/name  request_port:9000',
         'mqtt_port:1883  sensor_port:9001',
         'sensing_server_ip  ros2_bridge_topic'],
        ['+loadConfig(file)',
         '+preloadThresholds(db)',
         '+preloadOwners(db)',
         '+preloadMqttBindings(db)'],
        '#455A64', '#ECEFF1', fs=6.5)

# ════════════════ ROS2 boxes ════════════════
uml_box(ax, 19.4, 12.5, 5.8, 3.7,
        'robot_command_node',
        ['Sub: /plantmate/robot_command',
         'Pub: /goal_pose',
         'Pub: /controller/cmd_vel',
         'Sub: /odom'],
        ['+onRobotCommand(msg)',
         '+navigateTo(x, y)',
         '+publishGoalPose()',
         '+publishCmdVel()'],
        C_ROS_H, C_ROS)

uml_box(ax, 19.4, 9.0, 5.8, 3.2,
        'robot_heartbeat_node',
        ['Pub: device/arm/robot-1/status'],
        ['+publishHeartbeat()',
         '+updateOnlineStatus()',
         '+sendStatusToMqtt()'],
        C_ROS_H, C_ROS)

# ════════════════ DATABASE box ════════════════
uml_box(ax, 19.4, 0.7, 5.8, 7.3,
        'MySQL Database',
        ['users',
         'plants  (pos_x, pos_y, thresholds)',
         'sensor_data',
         'events / watering_logs',
         'mqtt_device_bindings',
         'mqtt_live_devices'],
        ['+PlantTable: CRUD',
         '+SensorLogTable: INSERT/SELECT',
         '+EventLogTable: INSERT/SELECT',
         '+RobotLogTable: INSERT/SELECT',
         '+UserTable: auth'],
        C_DB_H, C_DB)

# ════════════════ Arrows ════════════════
# App → Service
arrow(ax, 3.1, 14.5, 3.1, 13.2, 'binds', C_ANDROID_H)
# Service → Gateway (interface)
arrow(ax, 3.1, 9.8, 3.1, 9.52, 'uses', C_ANDROID_H)
# Gateway → Tcp
arrow(ax, 1.8, 7.0, 1.8, 6.72, 'implements', C_IFACE_H)
# Gateway → Mqtt
arrow(ax, 4.3, 7.0, 4.3, 6.72, 'implements', C_IFACE_H)
# Tcp → SocketClient
arrow(ax, 1.8, 4.2, 1.8, 3.9, 'uses', '#546E7A')
# Mqtt → MqttManager
arrow(ax, 4.3, 4.2, 4.3, 3.9, 'uses', '#546E7A')

# TCP Client → Server RequestThread (TCP 9000)
arrow(ax, 2.9, 2.3, 6.5, 15.0, 'TCP :9000', C_ANDROID_H, lw=1.5)
# MqttManager → MqttAdapter (MQTT 1883)
arrow(ax, 4.3, 2.3, 10.6, 11.75, 'MQTT :1883', C_SERVER_H, lw=1.5)

# RequestThread → DBQueue
arrow(ax, 10.3, 15.0, 10.6, 15.0, 'enqueue\nDB_REQ_CLIENT', C_SERVER_H)
# DBThread → MySQL
arrow(ax, 22.2, 8.0, 22.2, 8.5, '', C_DB_H)
arrow(ax, 14.4, 7.0, 19.4, 4.5, 'INSERT/SELECT', C_DB_H, lw=1.3)
arrow(ax, 10.6, 13.5, 10.6, 8.5,  # db_thread to db section label
      '', C_DB_H)

# SensorThread → DBThread (sensor data persist)
arrow(ax, 10.3, 11.75, 10.6, 14.5, 'DB_REQ_SENSOR', C_SERVER_H)
# SensorThread → Caches
arrow(ax, 8.4, 10.3, 8.4, 6.92, 'fill buffer\ncheck threshold', '#78909C')
# Caches → SensorEventDispatcher
arrow(ax, 10.3, 5.45, 10.6, 5.45, 'threshold\ncheck', '#78909C')
# SensorEventDispatcher → DB
arrow(ax, 14.4, 4.5, 19.4, 3.5, 'persist events', C_DB_H)

# RequestThread → WateringThread (enqueue command)
arrow(ax, 8.4, 13.5, 8.4, 10.02, 'enqueue\nWaterCmd', C_SERVER_H)
# WateringThread → ROS2Bridge
arrow(ax, 10.3, 8.6, 10.6, 8.6, 'publish\nrobot_cmd', C_ROS_H)
# ROS2Bridge (server) → ROS2 robot_command_node
arrow(ax, 14.4, 8.6, 19.4, 13.8, '/plantmate/\nrobot_command\n(MQTT)', C_ROS_H, lw=1.5)
# robot_command_node → heartbeat
arrow(ax, 22.3, 12.5, 22.3, 12.22, 'status', C_ROS_H)

# MqttAdapter → MqttDeviceRegistry update
arrow(ax, 12.5, 10.3, 14.4, 5.45, 'device\nstatus', '#546E7A')

# DB arrows for request_thread response
arrow(ax, 14.4, 15.0, 19.4, 5.0, 'SQL queries', C_DB_H, lw=1.3)

# ══════ Config note ══════
ax.text(13.0, 0.45,
        'Config: plantmate_config.json (host:192.168.0.6, tcp:9000, mqtt:1883)  |  '
        'ROS2 config: robot_config.json (goal_tolerance:0.2, max_speed:0.25 m/s)',
        ha='center', va='bottom', fontsize=6.5,
        color='#555555', style='italic')

plt.tight_layout(pad=0.5)
plt.savefig('/home/user/PlantMate/docs/system_design.png',
            dpi=180, bbox_inches='tight',
            facecolor='#F5F5F5')
print('Saved system_design.png')
