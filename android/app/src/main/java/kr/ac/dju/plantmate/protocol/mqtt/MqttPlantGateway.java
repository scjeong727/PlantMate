package kr.ac.dju.plantmate.protocol.mqtt;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

import kr.ac.dju.plantmate.model.EventRecord;
import kr.ac.dju.plantmate.model.MonitorSnapshot;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.model.SensorRecord;
import kr.ac.dju.plantmate.parser.ResponseParser;
import kr.ac.dju.plantmate.protocol.ConnectionConfig;
import kr.ac.dju.plantmate.protocol.PlantGateway;

public class MqttPlantGateway implements PlantGateway {

    private static final String PREFS = "mqtt_plant_repo";
    private static final String KEY_SELECTED_PLANT = "selected_plant";
    private static final String KEY_BROKER_HOST = "broker_host";
    private static final String KEY_BROKER_PORT = "broker_port";
    private static final String KEY_BROKER_CLIENT = "broker_client";
    private final SharedPreferences preferences;
    private final MqttManager mqttManager = new MqttManager();
    private final ResponseParser responseParser = new ResponseParser();
    private final List<String> sensorHistory = new ArrayList<>();
    private final List<String> eventHistory = new ArrayList<>();
    private final List<String> waterHistory = new ArrayList<>();
    private final Object rpcLock = new Object();

    private String loginId = "";
    private int userId = -1;
    private int selectedSensorPlantId = -1;
    private String responseTopic = "";
    private String pendingRequestId;
    private JSONObject pendingResponse;
    private CountDownLatch pendingLatch;

    private float lastTemperature;
    private float lastHumidity;
    private int lastSoil;
    private int lastLight;
    private String lastTimestamp = "";
    private String lastEventMessage = "대기 중";

    public MqttPlantGateway(Context context) {
        preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        mqttManager.setListener(new MqttManager.ManagerListener() {
            @Override
            public void onConnected(boolean reconnect) {
                appendEvent(reconnect ? "MQTT 재연결 완료" : "MQTT 브로커 연결됨");
            }

            @Override
            public void onConnectionLost(String message) {
                appendEvent(message);
            }

            @Override
            public void onMessageReceived(String topic, String payload) {
                handleIncomingMessage(topic, payload);
            }

            @Override
            public void onError(String message) {
                appendEvent(message);
            }
        });
    }

    @Override
    public void connect(ConnectionConfig config) throws Exception {
        if (config.getHost() == null || config.getHost().trim().isEmpty()) {
            throw new IllegalArgumentException("브로커 주소를 입력하세요.");
        }
        String clientId = config.getClientId() == null || config.getClientId().trim().isEmpty()
                ? "PlantMate-MQTT"
                : config.getClientId().trim();
        saveBrokerConfig(new BrokerConfig(
                config.getHost().trim(),
                config.getPort(),
                clientId
        ));
        responseTopic = MqttTopics.appResponseTopic(clientId);
        awaitAction(callback -> mqttManager.connect(getBrokerConfig(), callback));
        awaitAction(callback -> mqttManager.subscribe(responseTopic, 1, callback));
    }

    @Override
    public boolean isConnected() {
        return mqttManager.isConnected();
    }

    @Override
    public int login(String loginId, String loginPw) throws Exception {
        if (loginId == null || loginId.trim().isEmpty() || loginPw == null || loginPw.trim().isEmpty()) {
            throw new IllegalArgumentException("ID와 비밀번호를 입력하세요.");
        }
        this.loginId = loginId.trim();
        JSONObject response = sendRequest("login", request -> {
            request.put("loginId", this.loginId);
            request.put("password", loginPw.trim());
        });
        JSONObject data = response.optJSONObject("data");
        userId = data == null ? -1 : data.optInt("user_id", -1);
        if (userId <= 0) {
            throw new IllegalStateException("로그인 응답 파싱 실패");
        }
        appendEvent("MQTT 세션 로그인: " + this.loginId);
        return userId;
    }

    @Override
    public void signup(String loginId, String loginPw) throws Exception {
        if (loginId == null || loginId.trim().isEmpty() || loginPw == null || loginPw.trim().isEmpty()) {
            throw new IllegalArgumentException("ID와 비밀번호를 입력하세요.");
        }
        sendRequest("signup", request -> {
            request.put("loginId", loginId.trim());
            request.put("password", loginPw.trim());
        });
        appendEvent("MQTT 사용자 등록 완료: " + loginId.trim());
    }

    @Override
    public List<PlantProfile> loadPlants() throws Exception {
        JSONObject response = sendRequest("loadPlants", request -> request.put("userId", userId));
        JSONArray data = response.optJSONArray("data");
        return responseParser.parsePlants("OK " + (data == null ? "[]" : data.toString()));
    }

    @Override
    public void addPlant(int userId, PlantProfile plant) throws Exception {
        sendPlantRequest("addPlant", userId, plant);
        appendEvent("MQTT 식물 추가 완료");
    }

    @Override
    public void editPlant(int userId, PlantProfile plant) throws Exception {
        if (plant.getPlantId() <= 0) {
            throw new IllegalArgumentException("수정할 식물을 먼저 선택하세요.");
        }
        sendPlantRequest("editPlant", userId, plant);
        appendEvent("MQTT 식물 수정 완료");
    }

    @Override
    public void deletePlant(int userId, int plantId) throws Exception {
        if (plantId <= 0) {
            throw new IllegalArgumentException("삭제할 식물을 먼저 선택하세요.");
        }
        sendRequest("deletePlant", request -> {
            request.put("userId", userId);
            request.put("plantId", plantId);
        });
        if (selectedPlantId() == plantId) {
            selectPlant(-1);
        }
        appendEvent("MQTT 식물 삭제 완료");
    }

    @Override
    public MonitorSnapshot loadMonitorSnapshot(int plantId) throws Exception {
        selectPlant(plantId);
        SensorRecord recentSensor = responseParser.parseRecentSensor("OK " + requireData(sendRequest(
                "getRecentSensor",
                request -> request.put("plantId", plantId)
        )).toString());
        EventRecord recentEvent = responseParser.parseRecentEvent("OK " + requireData(sendRequest(
                "getRecentEvent",
                request -> request.put("plantId", plantId)
        )).toString());
        List<SensorRecord> sensorRecords = responseParser.parseSensorHistory("OK " + requireData(sendRequest(
                "getSensorHistory",
                request -> request.put("plantId", plantId)
        )).toString());
        List<EventRecord> eventRecords = responseParser.parseEventHistory("OK " + requireData(sendRequest(
                "getEventHistory",
                request -> request.put("plantId", plantId)
        )).toString());

        return new MonitorSnapshot(
                buildRecentSensorText(recentSensor),
                buildRecentEventText(recentEvent),
                buildSensorHistoryText(sensorRecords),
                buildEventHistoryText(eventRecords)
        );
    }

    @Override
    public List<String> loadSensorDevices() throws Exception {
        JSONObject response = sendRequest("getDeviceList", request -> request.put("deviceType", "sensor"));
        return responseParser.parseDeviceList("OK " + requireData(response).toString());
    }

    @Override
    public void setSensorDevice(String path, int plantId) throws Exception {
        requireConnected();
        if (path == null || path.trim().isEmpty()) {
            throw new IllegalArgumentException("센서 장치를 선택하세요.");
        }
        if (selectedSensorPlantId > 0) {
            unsubscribePlantTopics(selectedSensorPlantId);
        }
        sendRequest("bindDevice", request -> {
            request.put("plantId", plantId);
            request.put("role", "sensor");
            request.put("deviceType", "sensor");
            request.put("deviceId", path.trim());
        });
        selectedSensorPlantId = plantId;
        selectPlant(plantId);
        awaitAction(callback -> mqttManager.subscribe(MqttTopics.sensorTopic(plantId), 1, callback));
        awaitAction(callback -> mqttManager.subscribe(MqttTopics.statusTopic(plantId), 1, callback));
        appendEvent("MQTT 센서 장치 바인딩 완료: " + path.trim());
        appendEvent("MQTT 구독 시작: plant/" + plantId);
    }

    @Override
    public void stopSensorStream() throws Exception {
        if (selectedSensorPlantId <= 0) {
            return;
        }
        unsubscribePlantTopics(selectedSensorPlantId);
        appendEvent("MQTT 구독 중지");
        selectedSensorPlantId = -1;
    }

    @Override
    public List<String> loadWaterDevices() {
        try {
            JSONObject response = sendRequest("getDeviceList", request -> request.put("deviceType", "pump"));
            return responseParser.parseDeviceList("OK " + requireData(response).toString());
        } catch (Exception exception) {
            throw new IllegalStateException(exception.getMessage(), exception);
        }
    }

    @Override
    public void setWaterDevice(String path, int plantId) throws Exception {
        if (path == null || path.trim().isEmpty()) {
            throw new IllegalArgumentException("급수 장치를 선택하세요.");
        }
        if (plantId <= 0) {
            throw new IllegalStateException("식물을 먼저 선택하세요.");
        }

        sendRequest("bindDevice", request -> {
            request.put("plantId", plantId);
            request.put("role", "water");
            request.put("deviceType", "pump");
            request.put("deviceId", path.trim());
        });
        selectPlant(plantId);
        appendEvent("MQTT 급수 장치 바인딩 완료: " + path.trim());
    }

    @Override
    public void waterPlant(int plantId, int duration) throws Exception {
        requireConnected();
        if (duration <= 0) {
            throw new IllegalArgumentException("급수 시간을 입력하세요.");
        }
        JSONObject payload = new JSONObject();
        try {
            payload.put("duration", duration);
            payload.put("command", "water");
            payload.put("loginId", loginId);
        } catch (JSONException exception) {
            throw new IllegalStateException("급수 payload 생성 실패");
        }
        awaitAction(callback -> mqttManager.publish(MqttTopics.waterCommandTopic(plantId), payload.toString(), callback));
        String line = String.format(Locale.KOREA, "plant/%d | %d초", plantId, duration);
        waterHistory.add(0, line);
        appendEvent("MQTT 급수 명령 발행 완료");
    }

    @Override
    public List<String> loadRobotDevices() throws Exception {
        JSONObject response = sendRequest("getDeviceList", request -> request.put("deviceType", "robot"));
        return responseParser.parseDeviceList("OK " + requireData(response).toString());
    }

    @Override
    public void setRobotDevice(String deviceId, int plantId) throws Exception {
        requireConnected();
        if (deviceId == null || deviceId.trim().isEmpty()) {
            throw new IllegalArgumentException("로봇 장치를 선택하세요.");
        }
        if (plantId <= 0) {
            throw new IllegalArgumentException("식물을 선택하세요.");
        }

        sendRequest("bindDevice", request -> {
            request.put("plantId", plantId);
            request.put("role", "robot");
            request.put("deviceType", "robot");
            request.put("deviceId", deviceId.trim());
        });
        selectPlant(plantId);
        appendEvent("MQTT 로봇 장치 바인딩 완료: " + deviceId.trim());
    }

    @Override
    public void robotCommand(int plantId, String action, String detail) throws Exception {
        requireConnected();
        if (plantId <= 0) {
            throw new IllegalArgumentException("식물을 선택하세요.");
        }
        if (action == null || action.trim().isEmpty()) {
            throw new IllegalArgumentException("로봇 동작을 입력하세요.");
        }
        sendRequest("robotCommand", request -> {
            request.put("plantId", plantId);
            request.put("robotAction", action.trim());
            request.put("detail", detail == null ? "" : detail.trim());
        });
        appendEvent("MQTT 로봇 명령 전송 완료");
    }

    @Override
    public String loadWaterHistoryText(int plantId) throws Exception {
        List<EventRecord> events = responseParser.parseEventHistory("OK " + requireData(sendRequest(
                "getEventHistory",
                request -> request.put("plantId", plantId)
        )).toString());
        List<String> lines = new ArrayList<>();

        for (EventRecord event : events) {
            if (event.getEventType().contains("WATER")) {
                lines.add(event.getCreatedAt() + " | " + event.getEventType() + " | " + event.getMessage());
            }
        }

        return joinLines(lines, "급수 이력이 없습니다.");
    }

    @Override
    public void close() {
        mqttManager.disconnect();
    }

    private BrokerConfig getBrokerConfig() {
        return new BrokerConfig(
                preferences.getString(KEY_BROKER_HOST, "broker.hivemq.com"),
                preferences.getInt(KEY_BROKER_PORT, 1883),
                preferences.getString(KEY_BROKER_CLIENT, "PlantMate-MQTT")
        );
    }

    private void saveBrokerConfig(BrokerConfig config) {
        preferences.edit()
                .putString(KEY_BROKER_HOST, config.getHost())
                .putInt(KEY_BROKER_PORT, config.getPort())
                .putString(KEY_BROKER_CLIENT, config.getClientId())
                .apply();
    }

    private void handleIncomingMessage(String topic, String payload) {
        if (topic.equals(responseTopic)) {
            handleRpcResponse(payload);
            return;
        }

        int selectedPlantId = selectedPlantId();
        if (selectedPlantId <= 0) {
            return;
        }
        if (topic.equals(MqttTopics.sensorTopic(selectedPlantId))) {
            MqttSensorPayload sensorPayload = parseSensorPayload(payload);
            if (sensorPayload == null) {
                appendEvent("센서 payload 파싱 실패");
                return;
            }
            lastTemperature = sensorPayload.getTemperature();
            lastHumidity = sensorPayload.getHumidity();
            lastSoil = sensorPayload.getSoilMoisture();
            lastLight = sensorPayload.getLight();
            lastTimestamp = sensorPayload.getTimestamp();
            sensorHistory.add(0, formatSnapshotLine(sensorPayload));
            if (sensorHistory.size() > 20) {
                sensorHistory.remove(sensorHistory.size() - 1);
            }
            return;
        }
        if (topic.equals(MqttTopics.statusTopic(selectedPlantId))) {
            appendEvent(payload);
        }
    }

    private void handleRpcResponse(String payload) {
        try {
            JSONObject response = new JSONObject(payload);
            String requestId = response.optString("requestId", "");
            synchronized (rpcLock) {
                if (pendingLatch != null && requestId.equals(pendingRequestId)) {
                    pendingResponse = response;
                    pendingLatch.countDown();
                }
            }
        } catch (JSONException exception) {
            appendEvent("MQTT 응답 파싱 실패");
        }
    }

    private MqttSensorPayload parseSensorPayload(String payload) {
        try {
            JSONObject jsonObject = new JSONObject(payload);
            float temperature = (float) jsonObject.optDouble("temperature", jsonObject.optDouble("temp", 0));
            float humidity = (float) jsonObject.optDouble("humidity", jsonObject.optDouble("humi", 0));
            int soil = jsonObject.optInt("soilMoisture", jsonObject.optInt("soil", 0));
            int light = jsonObject.optInt("light", jsonObject.optInt("lux", 0));
            String timestamp = jsonObject.optString("timestamp", "");
            return new MqttSensorPayload(temperature, humidity, soil, light, timestamp);
        } catch (JSONException exception) {
            return null;
        }
    }

    private String formatSnapshotLine(MqttSensorPayload payload) {
        String prefix = payload.getTimestamp().isEmpty() ? "수신" : payload.getTimestamp();
        return String.format(Locale.KOREA, "%s | T %.1f / H %.1f / S %d / L %d",
                prefix,
                payload.getTemperature(),
                payload.getHumidity(),
                payload.getSoilMoisture(),
                payload.getLight());
    }

    private void appendEvent(String message) {
        lastEventMessage = message;
        eventHistory.add(0, message);
        if (eventHistory.size() > 20) {
            eventHistory.remove(eventHistory.size() - 1);
        }
    }

    private int selectedPlantId() {
        return preferences.getInt(KEY_SELECTED_PLANT, -1);
    }

    private void selectPlant(int plantId) {
        preferences.edit().putInt(KEY_SELECTED_PLANT, plantId).apply();
    }

    private String joinLines(List<String> lines, String emptyText) {
        if (lines.isEmpty()) {
            return emptyText;
        }
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < lines.size(); i++) {
            if (i > 0) {
                builder.append('\n');
            }
            builder.append(lines.get(i));
        }
        return builder.toString();
    }

    private void requireConnected() {
        if (!mqttManager.isConnected()) {
            throw new IllegalStateException("먼저 브로커에 연결하세요.");
        }
    }

    private void unsubscribePlantTopics(int plantId) throws Exception {
        awaitAction(callback -> mqttManager.unsubscribe(MqttTopics.sensorTopic(plantId), callback));
        awaitAction(callback -> mqttManager.unsubscribe(MqttTopics.statusTopic(plantId), callback));
    }

    private void sendPlantRequest(String action, int userId, PlantProfile plant) throws Exception {
        sendRequest(action, request -> {
            request.put("userId", userId);
            request.put("plantId", plant.getPlantId());
            request.put("name", plant.getName());
            request.put("type", plant.getType());
            request.put("positionX", plant.getPositionX() == null ? JSONObject.NULL : plant.getPositionX());
            request.put("positionY", plant.getPositionY() == null ? JSONObject.NULL : plant.getPositionY());
            request.put("tempMin", plant.getTempMin());
            request.put("tempMax", plant.getTempMax());
            request.put("humiMin", plant.getHumiMin());
            request.put("humiMax", plant.getHumiMax());
            request.put("soilMin", plant.getSoilMin());
            request.put("soilMax", plant.getSoilMax());
            request.put("lightMin", plant.getLightMin());
            request.put("lightMax", plant.getLightMax());
        });
    }

    private JSONObject sendRequest(String action, JsonCustomizer customizer) throws Exception {
        requireConnected();

        JSONObject request = new JSONObject();
        String requestId = String.valueOf(System.currentTimeMillis());
        try {
            request.put("requestId", requestId);
            request.put("action", action);
            customizer.customize(request);
        } catch (JSONException exception) {
            throw new IllegalStateException("MQTT 요청 생성 실패", exception);
        }

        CountDownLatch latch = new CountDownLatch(1);
        synchronized (rpcLock) {
            pendingRequestId = requestId;
            pendingResponse = null;
            pendingLatch = latch;
        }

        try {
            awaitAction(callback -> mqttManager.publish(MqttTopics.appRequestTopic(getBrokerConfig().getClientId()), request.toString(), callback));
            if (!latch.await(10, TimeUnit.SECONDS)) {
                throw new IOException("MQTT 응답 대기 시간 초과");
            }
        } finally {
            synchronized (rpcLock) {
                pendingLatch = null;
            }
        }

        if (pendingResponse == null) {
            throw new IOException("MQTT 응답 없음");
        }
        if (!pendingResponse.optBoolean("ok", false)) {
            throw new IOException(pendingResponse.optString("error", "request_failed"));
        }
        return pendingResponse;
    }

    private Object requireData(JSONObject response) {
        Object data = response.opt("data");
        if (data == null) {
            throw new IllegalStateException("응답 데이터가 없습니다.");
        }
        return data;
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
        return joinLines(lines, "센서 이력이 없습니다.");
    }

    private String buildEventHistoryText(List<EventRecord> history) {
        List<String> lines = new ArrayList<>();
        for (EventRecord item : history) {
            lines.add(item.getCreatedAt()
                    + " | " + item.getEventType()
                    + " | " + item.getMessage());
        }
        return joinLines(lines, "이벤트 이력이 없습니다.");
    }

    private void awaitAction(ActionRunner runner) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        AtomicReference<String> errorRef = new AtomicReference<>();
        runner.run(new MqttManager.ActionCallback() {
            @Override
            public void onSuccess() {
                latch.countDown();
            }

            @Override
            public void onError(String message) {
                errorRef.set(message);
                latch.countDown();
            }
        });
        if (!latch.await(10, TimeUnit.SECONDS)) {
            throw new IOException("MQTT 응답 대기 시간 초과");
        }
        if (errorRef.get() != null) {
            throw new IOException(errorRef.get());
        }
    }

    private interface ActionRunner {
        void run(MqttManager.ActionCallback callback) throws Exception;
    }

    private interface JsonCustomizer {
        void customize(JSONObject request) throws JSONException;
    }
}
