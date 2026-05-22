package kr.ac.dju.plantmate.model;

public class SensorRecord {

    private final String createdAt;
    private final String temp;
    private final String humi;
    private final String soil;
    private final String light;

    public SensorRecord(String createdAt, String temp, String humi, String soil, String light) {
        this.createdAt = createdAt;
        this.temp = temp;
        this.humi = humi;
        this.soil = soil;
        this.light = light;
    }

    public String getCreatedAt() {
        return createdAt;
    }

    public String getTemp() {
        return temp;
    }

    public String getHumi() {
        return humi;
    }

    public String getSoil() {
        return soil;
    }

    public String getLight() {
        return light;
    }
}
