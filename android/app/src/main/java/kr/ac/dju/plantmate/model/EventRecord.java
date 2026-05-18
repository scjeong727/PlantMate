package kr.ac.dju.plantmate.model;

public class EventRecord {

    private final String createdAt;
    private final String eventType;
    private final String message;
    private final String plantId;

    public EventRecord(String createdAt, String eventType, String message, String plantId) {
        this.createdAt = createdAt;
        this.eventType = eventType;
        this.message = message;
        this.plantId = plantId;
    }

    public String getCreatedAt() {
        return createdAt;
    }

    public String getEventType() {
        return eventType;
    }

    public String getMessage() {
        return message;
    }

    public String getPlantId() {
        return plantId;
    }
}
