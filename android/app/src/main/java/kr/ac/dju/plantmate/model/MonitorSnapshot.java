package kr.ac.dju.plantmate.model;

public class MonitorSnapshot {

    private final String recentSensorText;
    private final String recentEventText;
    private final String sensorHistoryText;
    private final String eventHistoryText;

    public MonitorSnapshot(
            String recentSensorText,
            String recentEventText,
            String sensorHistoryText,
            String eventHistoryText
    ) {
        this.recentSensorText = recentSensorText;
        this.recentEventText = recentEventText;
        this.sensorHistoryText = sensorHistoryText;
        this.eventHistoryText = eventHistoryText;
    }

    public String getRecentSensorText() {
        return recentSensorText;
    }

    public String getRecentEventText() {
        return recentEventText;
    }

    public String getSensorHistoryText() {
        return sensorHistoryText;
    }

    public String getEventHistoryText() {
        return eventHistoryText;
    }
}
