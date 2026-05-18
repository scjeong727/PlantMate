package kr.ac.dju.plantmate.repository;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import kr.ac.dju.plantmate.model.EventRecord;
import kr.ac.dju.plantmate.model.MonitorSnapshot;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.model.SensorRecord;
import kr.ac.dju.plantmate.network.SocketCommandClient;
import kr.ac.dju.plantmate.parser.ResponseParser;

public class PlantRepository {

    private final SocketCommandClient socketClient;
    private final ResponseParser parser;

    public PlantRepository(SocketCommandClient socketClient, ResponseParser parser) {
        this.socketClient = socketClient;
        this.parser = parser;
    }

    public void connect(String host, int port) throws IOException {
        if (host == null || host.trim().isEmpty()) {
            throw new IllegalArgumentException("서버 주소를 입력하세요.");
        }
        socketClient.connect(host.trim(), port);
    }

    public int login(String loginId, String loginPw) throws IOException {
        validateAuth(loginId, loginPw);
        String response = socketClient.sendCommand("LOGIN " + loginId + " " + loginPw);
        parser.requireOk(response);
        int userId = parser.parseUserId(response);
        if (userId <= 0) {
            throw new IllegalStateException("로그인 응답 파싱 실패");
        }
        return userId;
    }

    public void signup(String loginId, String loginPw) throws IOException {
        validateAuth(loginId, loginPw);
        String response = socketClient.sendCommand("ADD_USER " + loginId + " " + loginPw + " " + loginId);
        parser.requireOk(response);
    }

    public List<PlantProfile> loadPlants() throws IOException {
        String response = socketClient.sendCommand("GET_PLANT_BY_USER");
        return parser.parsePlants(response);
    }

    public void addPlant(int userId, PlantProfile plant) throws IOException {
        validatePlant(plant);
        String request = String.format(
                Locale.US,
                "ADD_PLANT %d %s %s %.2f %.2f %.2f %.2f %d %d %d %d",
                userId,
                plant.getName(),
                plant.getType(),
                plant.getTempMin(),
                plant.getTempMax(),
                plant.getHumiMin(),
                plant.getHumiMax(),
                plant.getSoilMin(),
                plant.getSoilMax(),
                plant.getLightMin(),
                plant.getLightMax()
        );
        parser.requireOk(socketClient.sendCommand(request));
    }

    public void editPlant(int userId, PlantProfile plant) throws IOException {
        if (plant.getPlantId() <= 0) {
            throw new IllegalArgumentException("수정할 식물을 먼저 선택하세요.");
        }
        validatePlant(plant);

        String request = String.format(
                Locale.US,
                "EDIT_PLANT %d %d %s %s %.2f %.2f %.2f %.2f %d %d %d %d",
                plant.getPlantId(),
                userId,
                plant.getName(),
                plant.getType(),
                plant.getTempMin(),
                plant.getTempMax(),
                plant.getHumiMin(),
                plant.getHumiMax(),
                plant.getSoilMin(),
                plant.getSoilMax(),
                plant.getLightMin(),
                plant.getLightMax()
        );
        parser.requireOk(socketClient.sendCommand(request));
    }

    public void deletePlant(int userId, int plantId) throws IOException {
        if (plantId <= 0) {
            throw new IllegalArgumentException("삭제할 식물을 먼저 선택하세요.");
        }
        parser.requireOk(socketClient.sendCommand("REMOVE_PLANT " + plantId + " " + userId));
    }

    public MonitorSnapshot loadMonitorSnapshot(int plantId) throws IOException {
        if (plantId <= 0) {
            throw new IllegalArgumentException("식물을 선택하세요.");
        }

        SensorRecord recentSensor = parser.parseRecentSensor(
                socketClient.sendCommand("GET_RECENT_SENSOR_BY_PLANT " + plantId)
        );
        EventRecord recentEvent = parser.parseRecentEvent(
                socketClient.sendCommand("GET_RECENT_EVENT_BY_PLANT " + plantId)
        );
        List<SensorRecord> sensorHistory = parser.parseSensorHistory(
                socketClient.sendCommand("GET_SENSOR_LIST_BY_PLANT " + plantId + " 10")
        );
        List<EventRecord> eventHistory = parser.parseEventHistory(
                socketClient.sendCommand("GET_EVENT_LIST_BY_PLANT " + plantId + " 10")
        );

        return new MonitorSnapshot(
                buildRecentSensorText(recentSensor),
                buildRecentEventText(recentEvent),
                buildSensorHistoryText(sensorHistory),
                buildEventHistoryText(eventHistory)
        );
    }

    public List<String> loadSensorDevices() throws IOException {
        return parser.parseDeviceList(socketClient.sendCommand("GET_SENSOR_DEVICE_LIST"));
    }

    public void setSensorDevice(String path, int plantId) throws IOException {
        if (path == null || path.trim().isEmpty()) {
            throw new IllegalArgumentException("센서 장치를 선택하세요.");
        }
        parser.requireOk(socketClient.sendCommand("SET_SENSOR_DEVICE " + path));
        parser.requireOk(socketClient.sendCommand("START_SENSOR_STREAM " + plantId));
    }

    public void stopSensorStream() throws IOException {
        parser.requireOk(socketClient.sendCommand("STOP_SENSOR_STREAM"));
    }

    public List<String> loadWaterDevices() throws IOException {
        return parser.parseDeviceList(socketClient.sendCommand("GET_WATER_DEVICE_LIST"));
    }

    public void setWaterDevice(String path) throws IOException {
        if (path == null || path.trim().isEmpty()) {
            throw new IllegalArgumentException("모터 장치를 선택하세요.");
        }
        parser.requireOk(socketClient.sendCommand("SET_WATER_DEVICE " + path));
    }

    public void waterPlant(int plantId, int duration) throws IOException {
        if (plantId <= 0) {
            throw new IllegalArgumentException("식물을 선택하세요.");
        }
        if (duration <= 0) {
            throw new IllegalArgumentException("급수 시간을 입력하세요.");
        }
        parser.requireOk(socketClient.sendCommand("WATER_PLANT " + plantId + " " + duration));
    }

    public String loadWaterHistoryText(int plantId) throws IOException {
        List<EventRecord> events = parser.parseEventHistory(
                socketClient.sendCommand("GET_EVENT_LIST_BY_PLANT " + plantId + " 20")
        );
        List<String> lines = new ArrayList<>();

        for (EventRecord event : events) {
            if (event.getEventType().contains("WATER")) {
                lines.add(event.getCreatedAt() + " | " + event.getEventType() + " | " + event.getMessage());
            }
        }

        return lines.isEmpty() ? "급수 이력이 없습니다." : joinLines(lines);
    }

    private void validateAuth(String loginId, String loginPw) {
        if (loginId == null || loginId.trim().isEmpty() || loginPw == null || loginPw.trim().isEmpty()) {
            throw new IllegalArgumentException("ID와 비밀번호를 입력하세요.");
        }
    }

    private void validatePlant(PlantProfile plant) {
        if (plant.getName() == null || plant.getName().trim().isEmpty()
                || plant.getType() == null || plant.getType().trim().isEmpty()) {
            throw new IllegalArgumentException("식물 이름과 종류를 입력하세요.");
        }
    }

    private String buildRecentSensorText(SensorRecord sensor) {
        return "T:" + sensor.getTemp()
                + " H:" + sensor.getHumi()
                + " S:" + sensor.getSoil()
                + " L:" + sensor.getLight();
    }

    private String buildRecentEventText(EventRecord event) {
        return event.getEventType() + " : " + event.getMessage();
    }

    private String buildSensorHistoryText(List<SensorRecord> history) {
        List<String> lines = new ArrayList<>();
        for (SensorRecord item : history) {
            lines.add(item.getCreatedAt()
                    + " | T:" + item.getTemp()
                    + " H:" + item.getHumi()
                    + " S:" + item.getSoil()
                    + " L:" + item.getLight());
        }
        return lines.isEmpty() ? "센서 이력이 없습니다." : joinLines(lines);
    }

    private String buildEventHistoryText(List<EventRecord> history) {
        List<String> lines = new ArrayList<>();
        for (EventRecord item : history) {
            lines.add(item.getCreatedAt()
                    + " | " + item.getEventType()
                    + " | " + item.getMessage());
        }
        return lines.isEmpty() ? "이벤트 이력이 없습니다." : joinLines(lines);
    }

    private String joinLines(List<String> lines) {
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < lines.size(); i++) {
            if (i > 0) {
                builder.append('\n');
            }
            builder.append(lines.get(i));
        }
        return builder.toString();
    }
}
