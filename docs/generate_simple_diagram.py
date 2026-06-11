import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
from matplotlib import font_manager

# Load NanumGothic
font_path = '/usr/share/fonts/truetype/nanum/NanumBarunGothic.ttf'
font_manager.fontManager.addfont(font_path)
plt.rcParams['font.family'] = 'NanumBarunGothic'
plt.rcParams['axes.unicode_minus'] = False

fig, ax = plt.subplots(figsize=(20, 12))
ax.set_xlim(0, 20)
ax.set_ylim(0, 12)
ax.axis('off')
fig.patch.set_facecolor('#FAFAFA')

# Colors
BLUE   = '#1565C0'; LB = '#E3F2FD'
ORANGE = '#E65100'; LO = '#FFF3E0'
PURPLE = '#6A1B9A'; LP = '#F3E5F5'
GREEN  = '#2E7D32'; LG = '#E8F5E9'
GRAY   = '#455A64'; LGR = '#ECEFF1'

def box(ax, x, y, w, h, title, lines, hc, bc):
    ax.add_patch(FancyBboxPatch((x+0.08, y-0.08), w, h,
        boxstyle="round,pad=0.15", lw=0, fc='#C8C8C8', zorder=1))
    ax.add_patch(FancyBboxPatch((x, y), w, h,
        boxstyle="round,pad=0.15", lw=2, ec=hc, fc=bc, zorder=2))
    hh = 0.72
    ax.add_patch(FancyBboxPatch((x, y+h-hh), w, hh,
        boxstyle="round,pad=0.15", lw=0, fc=hc, zorder=3))
    ax.text(x+w/2, y+h-hh/2, title,
            ha='center', va='center', fontsize=13,
            fontweight='bold', color='white', zorder=4)
    # divider
    ax.plot([x+0.2, x+w-0.2], [y+h-hh, y+h-hh], color=hc, lw=0.8, zorder=4)
    n = len(lines)
    usable = h - hh - 0.3
    step = usable / max(n, 1)
    for i, line in enumerate(lines):
        ly = y + h - hh - 0.18 - i * step
        ax.text(x+w/2, ly, line,
                ha='center', va='top',
                fontsize=9.5, color='#2B2B2B', zorder=4)

def bidir(ax, x1, y1, x2, y2, label, color):
    ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle='<->', color=color, lw=2.2),
                zorder=10)
    mx, my = (x1+x2)/2, (y1+y2)/2
    ax.text(mx, my, label, ha='center', va='center',
            fontsize=8.5, color=color, fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.3', fc='white', ec=color, lw=1.2, alpha=0.95),
            zorder=11)

def arr(ax, x1, y1, x2, y2, label, color, dashed=False):
    ls = (0, (5, 4)) if dashed else 'solid'
    ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle='->', color=color, lw=2.0, linestyle=ls),
                zorder=10)
    mx, my = (x1+x2)/2, (y1+y2)/2
    ax.text(mx, my, label, ha='center', va='center',
            fontsize=8.5, color=color, fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.3', fc='white', ec=color, lw=1.2, alpha=0.95),
            zorder=11)

# ── Title ──────────────────────────────────────────────────────────
ax.text(10, 11.55, 'PlantMate  시스템 구조',
        ha='center', va='center', fontsize=21, fontweight='bold', color='#263238')
ax.plot([1.5, 18.5], [11.15, 11.15], color='#B0BEC5', lw=1.5)

# ── Boxes ──────────────────────────────────────────────────────────
# Android App (left)
box(ax, 0.3, 5.5, 4.5, 5.2, 'Android App',
    ['LoginActivity',
     'PlantClientService',
     'PlantGateway (interface)',
     '  TcpPlantGateway',
     '  MqttPlantGateway'],
    BLUE, LB)

# C Server (center)
box(ax, 5.6, 1.5, 5.6, 9.2, 'C Server',
    ['RequestThread  :9000 (TCP)',
     'MqttAdapter       :1883 (MQTT)',
     'SensorThread    :9001 (UDP)',
     '──────────────────',
     'DBThread  +  DBQueue',
     'WateringThread',
     'ROS2Bridge',
     '──────────────────',
     'SensorBuffer  (256)',
     'ThresholdCache  (256)',
     'EventLog  (128)'],
    ORANGE, LO)

# MySQL (bottom right)
box(ax, 13.0, 1.5, 4.2, 4.5, 'MySQL DB',
    ['users  /  plants',
     'sensor_data  /  events',
     'watering_logs',
     'mqtt_device_bindings'],
    PURPLE, LP)

# ROS2 Robot (top right)
box(ax, 13.0, 7.0, 4.2, 3.7, 'ROS2  (JetRover)',
    ['robot_command_node',
     '  /goal_pose',
     '  /controller/cmd_vel',
     'robot_heartbeat_node'],
    GREEN, LG)

# IoT Sensor (bottom left)
box(ax, 0.3, 1.5, 4.5, 3.5, '센서 장치  (IoT)',
    ['온도 / 습도 / 토양 / 조도',
     'MQTT or UDP 전송'],
    GRAY, LGR)

# ── Arrows ─────────────────────────────────────────────────────────
# Android <-> Server
bidir(ax, 4.8, 9.2, 5.6, 9.2, 'TCP :9000',   BLUE)
bidir(ax, 4.8, 8.0, 5.6, 8.0, 'MQTT :1883',  BLUE)

# Server -> DB
arr(ax, 11.2, 3.8, 13.0, 3.5, 'SQL 쿼리', PURPLE)

# Server -> ROS2
arr(ax, 11.2, 8.0, 13.0, 8.5,
    '/plantmate/robot_command\n(MQTT)', GREEN)

# Sensor -> Server
arr(ax, 4.8, 3.2, 5.6, 5.0, 'UDP :9001', GRAY, dashed=True)

# ROS2 -> DB (log)
arr(ax, 15.1, 7.0, 15.1, 6.0, '상태 로그', PURPLE, dashed=True)

plt.tight_layout(pad=0.3)
plt.savefig('/home/user/PlantMate/docs/system_design_simple.png',
            dpi=180, bbox_inches='tight', facecolor='#FAFAFA')
print('Done')
