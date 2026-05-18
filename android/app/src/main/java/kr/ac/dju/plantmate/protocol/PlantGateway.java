package kr.ac.dju.plantmate.protocol;

import java.io.IOException;
import java.util.List;

import kr.ac.dju.plantmate.model.MonitorSnapshot;
import kr.ac.dju.plantmate.model.PlantProfile;

public interface PlantGateway {

    void connect(ConnectionConfig config) throws Exception;

    boolean isConnected();

    int login(String loginId, String loginPw) throws Exception;

    void signup(String loginId, String loginPw) throws Exception;

    List<PlantProfile> loadPlants() throws Exception;

    void addPlant(int userId, PlantProfile plant) throws Exception;

    void editPlant(int userId, PlantProfile plant) throws Exception;

    void deletePlant(int userId, int plantId) throws Exception;

    MonitorSnapshot loadMonitorSnapshot(int plantId) throws Exception;

    List<String> loadSensorDevices() throws Exception;

    void setSensorDevice(String path, int plantId) throws Exception;

    void stopSensorStream() throws Exception;

    List<String> loadWaterDevices() throws Exception;

    void setWaterDevice(String path, int plantId) throws Exception;

    void waterPlant(int plantId, int duration) throws Exception;

    String loadWaterHistoryText(int plantId) throws Exception;

    void close() throws IOException;
}
