package kr.ac.dju.plantmate.protocol.mqtt;

public final class MqttTopics {

    private MqttTopics() {
    }

    public static String sensorTopic(int plantId) {
        return "plant/" + plantId + "/sensor";
    }

    public static String statusTopic(int plantId) {
        return "plant/" + plantId + "/status";
    }

    public static String waterCommandTopic(int plantId) {
        return "plant/" + plantId + "/water/command";
    }

    public static String deviceCommandTopic(String deviceType, String deviceId, String action) {
        return "device/" + deviceType + "/" + deviceId + "/" + action + "/command";
    }

    public static String deviceStatusTopic(String deviceType, String deviceId) {
        return "device/" + deviceType + "/" + deviceId + "/status";
    }

    public static String appRequestTopic(String clientId) {
        return "app/" + clientId + "/request";
    }

    public static String appResponseTopic(String clientId) {
        return "app/" + clientId + "/response";
    }
}
