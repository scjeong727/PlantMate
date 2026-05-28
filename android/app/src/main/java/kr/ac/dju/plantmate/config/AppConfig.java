package kr.ac.dju.plantmate.config;

import android.content.Context;

import org.json.JSONObject;

import java.io.InputStream;
import java.nio.charset.StandardCharsets;

public class AppConfig {

    private static final String CONFIG_FILE = "plantmate_config.json";

    private final String defaultHost;
    private final int mqttPort;
    private final int tcpPort;
    private final String clientId;

    private AppConfig(String defaultHost, int mqttPort, int tcpPort, String clientId) {
        this.defaultHost = defaultHost;
        this.mqttPort = mqttPort;
        this.tcpPort = tcpPort;
        this.clientId = clientId;
    }

    public static AppConfig load(Context context) {
        String defaultHost = "192.168.0.6";
        int mqttPort = 1883;
        int tcpPort = 9000;
        String clientId = "PlantMate-Android";

        try (InputStream inputStream = context.getAssets().open(CONFIG_FILE)) {
            byte[] buffer = new byte[inputStream.available()];
            int read = inputStream.read(buffer);
            if (read > 0) {
                JSONObject json = new JSONObject(new String(buffer, 0, read, StandardCharsets.UTF_8));
                defaultHost = json.optString("defaultHost", defaultHost);
                mqttPort = json.optInt("mqttPort", mqttPort);
                tcpPort = json.optInt("tcpPort", tcpPort);
                clientId = json.optString("clientId", clientId);
            }
        } catch (Exception ignored) {
        }

        return new AppConfig(defaultHost, mqttPort, tcpPort, clientId);
    }

    public String getDefaultHost() {
        return defaultHost;
    }

    public int getMqttPort() {
        return mqttPort;
    }

    public int getTcpPort() {
        return tcpPort;
    }

    public String getClientId() {
        return clientId;
    }
}
