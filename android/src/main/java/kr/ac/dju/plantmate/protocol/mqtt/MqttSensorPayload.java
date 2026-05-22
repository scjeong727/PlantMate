package kr.ac.dju.plantmate.protocol.mqtt;

public class MqttSensorPayload {

    private final float temperature;
    private final float humidity;
    private final int soilMoisture;
    private final int light;
    private final String timestamp;

    public MqttSensorPayload(float temperature, float humidity, int soilMoisture, int light, String timestamp) {
        this.temperature = temperature;
        this.humidity = humidity;
        this.soilMoisture = soilMoisture;
        this.light = light;
        this.timestamp = timestamp;
    }

    public float getTemperature() {
        return temperature;
    }

    public float getHumidity() {
        return humidity;
    }

    public int getSoilMoisture() {
        return soilMoisture;
    }

    public int getLight() {
        return light;
    }

    public String getTimestamp() {
        return timestamp;
    }
}
