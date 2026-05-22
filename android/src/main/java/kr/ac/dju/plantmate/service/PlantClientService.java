package kr.ac.dju.plantmate.service;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import kr.ac.dju.plantmate.model.MonitorSnapshot;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.model.SessionState;
import kr.ac.dju.plantmate.protocol.ConnectionConfig;
import kr.ac.dju.plantmate.protocol.PlantGateway;
import kr.ac.dju.plantmate.protocol.ProtocolType;
import kr.ac.dju.plantmate.protocol.mqtt.MqttPlantGateway;
import kr.ac.dju.plantmate.protocol.tcp.TcpPlantGateway;

public class PlantClientService extends Service {

    public interface ServiceCallback<T> {
        void onSuccess(T data);
        void onError(String message);
    }

    public class LocalBinder extends Binder {
        public PlantClientService getService() {
            return PlantClientService.this;
        }
    }

    private final IBinder binder = new LocalBinder();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();

    private boolean loggedIn;
    private int userId = -1;
    private String loginId = "";
    private boolean sensorStreaming;
    private ProtocolType protocolType = ProtocolType.TCP;
    private PlantGateway gateway;

    @Override
    public void onCreate() {
        super.onCreate();
        gateway = createGateway(protocolType);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    @Override
    public void onDestroy() {
        worker.execute(() -> {
            try {
                if (sensorStreaming) {
                    gateway.stopSensorStream();
                }
                gateway.close();
            } catch (IOException ignored) {
            } catch (Exception ignored) {
            }
        });
        worker.shutdown();
        super.onDestroy();
    }

    public SessionState getSessionState() {
        return new SessionState(gateway.isConnected(), loggedIn, userId, loginId, protocolType);
    }

    public void connect(ConnectionConfig config, ServiceCallback<SessionState> callback) {
        runTask(callback, () -> {
            switchGateway(config.getProtocolType());
            gateway.connect(config);
            return getSessionState();
        });
    }

    public void login(String id, String pw, ServiceCallback<SessionState> callback) {
        runTask(callback, () -> {
            int loginUserId = gateway.login(id, pw);
            loggedIn = true;
            userId = loginUserId;
            loginId = id;
            return getSessionState();
        });
    }

    public void signup(String id, String pw, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            gateway.signup(id, pw);
            return "회원가입 완료";
        });
    }

    public void loadPlants(ServiceCallback<List<PlantProfile>> callback) {
        runTask(callback, this::requireLoginAndLoadPlants);
    }

    public void addPlant(PlantProfile plant, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.addPlant(userId, plant);
            return "식물 추가 완료";
        });
    }

    public void editPlant(PlantProfile plant, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.editPlant(userId, plant);
            return "식물 수정 완료";
        });
    }

    public void deletePlant(int plantId, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.deletePlant(userId, plantId);
            return "식물 삭제 완료";
        });
    }

    public void loadMonitorSnapshot(int plantId, ServiceCallback<MonitorSnapshot> callback) {
        runTask(callback, () -> {
            requireLogin();
            return gateway.loadMonitorSnapshot(plantId);
        });
    }

    public void loadSensorDevices(ServiceCallback<List<String>> callback) {
        runTask(callback, () -> {
            requireLogin();
            return gateway.loadSensorDevices();
        });
    }

    public void setSensorDevice(String path, int plantId, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.setSensorDevice(path, plantId);
            sensorStreaming = true;
            return "센서 장치 연결 완료";
        });
    }

    public void loadWaterDevices(ServiceCallback<List<String>> callback) {
        runTask(callback, () -> {
            requireLogin();
            return gateway.loadWaterDevices();
        });
    }

    public void setWaterDevice(String path, int plantId, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.setWaterDevice(path, plantId);
            return "모터 장치 설정 완료";
        });
    }

    public void waterPlant(int plantId, int duration, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.waterPlant(plantId, duration);
            return "급수 요청 완료";
        });
    }

    public void loadRobotDevices(ServiceCallback<List<String>> callback) {
        runTask(callback, () -> {
            requireLogin();
            return gateway.loadRobotDevices();
        });
    }

    public void setRobotDevice(String deviceId, int plantId, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.setRobotDevice(deviceId, plantId);
            return "로봇 장치 연결 완료";
        });
    }

    public void robotCommand(int plantId, String action, String detail, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.robotCommand(plantId, action, detail);
            return "로봇 명령 전송 완료";
        });
    }

    public void loadWaterHistory(int plantId, ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            return gateway.loadWaterHistoryText(plantId);
        });
    }

    public void stopSensorStream(ServiceCallback<String> callback) {
        runTask(callback, () -> {
            requireLogin();
            gateway.stopSensorStream();
            sensorStreaming = false;
            return "센서 스트림 중지 완료";
        });
    }

    private List<PlantProfile> requireLoginAndLoadPlants() throws IOException {
        requireLogin();
        try {
            return gateway.loadPlants();
        } catch (IOException exception) {
            throw exception;
        } catch (Exception exception) {
            throw new IOException(exception.getMessage(), exception);
        }
    }

    private void requireLogin() {
        if (!loggedIn || userId <= 0) {
            throw new IllegalStateException("먼저 로그인하세요.");
        }
    }

    private <T> void runTask(ServiceCallback<T> callback, ServiceWork<T> work) {
        worker.execute(() -> {
            try {
                T result = work.run();
                mainHandler.post(() -> callback.onSuccess(result));
            } catch (Exception e) {
                String message = e.getMessage() == null ? "처리 실패" : e.getMessage();
                mainHandler.post(() -> callback.onError(message));
            }
        });
    }

    private interface ServiceWork<T> {
        T run() throws Exception;
    }

    private void switchGateway(ProtocolType nextProtocol) throws Exception {
        if (gateway != null && protocolType == nextProtocol) {
            return;
        }
        if (gateway != null) {
            gateway.close();
        }
        protocolType = nextProtocol;
        gateway = createGateway(nextProtocol);
        loggedIn = false;
        userId = -1;
        loginId = "";
        sensorStreaming = false;
    }

    private PlantGateway createGateway(ProtocolType protocolType) {
        if (protocolType == ProtocolType.MQTT) {
            return new MqttPlantGateway(getApplicationContext());
        }
        return new TcpPlantGateway();
    }
}
