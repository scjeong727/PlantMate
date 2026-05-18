package kr.ac.dju.plantmate.protocol.tcp;

import java.io.IOException;
import java.util.List;

import kr.ac.dju.plantmate.model.MonitorSnapshot;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.network.SocketCommandClient;
import kr.ac.dju.plantmate.parser.ResponseParser;
import kr.ac.dju.plantmate.protocol.ConnectionConfig;
import kr.ac.dju.plantmate.protocol.PlantGateway;
import kr.ac.dju.plantmate.repository.PlantRepository;

public class TcpPlantGateway implements PlantGateway {

    private final SocketCommandClient socketClient = new SocketCommandClient();
    private final PlantRepository repository = new PlantRepository(socketClient, new ResponseParser());

    @Override
    public void connect(ConnectionConfig config) throws Exception {
        repository.connect(config.getHost(), config.getPort());
    }

    @Override
    public boolean isConnected() {
        return socketClient.isConnected();
    }

    @Override
    public int login(String loginId, String loginPw) throws Exception {
        return repository.login(loginId, loginPw);
    }

    @Override
    public void signup(String loginId, String loginPw) throws Exception {
        repository.signup(loginId, loginPw);
    }

    @Override
    public List<PlantProfile> loadPlants() throws Exception {
        return repository.loadPlants();
    }

    @Override
    public void addPlant(int userId, PlantProfile plant) throws Exception {
        repository.addPlant(userId, plant);
    }

    @Override
    public void editPlant(int userId, PlantProfile plant) throws Exception {
        repository.editPlant(userId, plant);
    }

    @Override
    public void deletePlant(int userId, int plantId) throws Exception {
        repository.deletePlant(userId, plantId);
    }

    @Override
    public MonitorSnapshot loadMonitorSnapshot(int plantId) throws Exception {
        return repository.loadMonitorSnapshot(plantId);
    }

    @Override
    public List<String> loadSensorDevices() throws Exception {
        return repository.loadSensorDevices();
    }

    @Override
    public void setSensorDevice(String path, int plantId) throws Exception {
        repository.setSensorDevice(path, plantId);
    }

    @Override
    public void stopSensorStream() throws Exception {
        repository.stopSensorStream();
    }

    @Override
    public List<String> loadWaterDevices() throws Exception {
        return repository.loadWaterDevices();
    }

    @Override
    public void setWaterDevice(String path, int plantId) throws Exception {
        repository.setWaterDevice(path);
    }

    @Override
    public void waterPlant(int plantId, int duration) throws Exception {
        repository.waterPlant(plantId, duration);
    }

    @Override
    public String loadWaterHistoryText(int plantId) throws Exception {
        return repository.loadWaterHistoryText(plantId);
    }

    @Override
    public void close() throws IOException {
        socketClient.close();
    }
}
