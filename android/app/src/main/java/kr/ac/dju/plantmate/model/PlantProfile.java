package kr.ac.dju.plantmate.model;

public class PlantProfile {

    private final int plantId;
    private final int userId;
    private final String name;
    private final String type;
    private final Double positionX;
    private final Double positionY;
    private final double tempMin;
    private final double tempMax;
    private final double humiMin;
    private final double humiMax;
    private final int soilMin;
    private final int soilMax;
    private final int lightMin;
    private final int lightMax;
    private final String createdAt;

    public PlantProfile(
            int plantId,
            int userId,
            String name,
            String type,
            Double positionX,
            Double positionY,
            double tempMin,
            double tempMax,
            double humiMin,
            double humiMax,
            int soilMin,
            int soilMax,
            int lightMin,
            int lightMax,
            String createdAt
    ) {
        this.plantId = plantId;
        this.userId = userId;
        this.name = name;
        this.type = type;
        this.positionX = positionX;
        this.positionY = positionY;
        this.tempMin = tempMin;
        this.tempMax = tempMax;
        this.humiMin = humiMin;
        this.humiMax = humiMax;
        this.soilMin = soilMin;
        this.soilMax = soilMax;
        this.lightMin = lightMin;
        this.lightMax = lightMax;
        this.createdAt = createdAt;
    }

    public int getPlantId() {
        return plantId;
    }

    public int getUserId() {
        return userId;
    }

    public String getName() {
        return name;
    }

    public String getType() {
        return type;
    }

    public Double getPositionX() {
        return positionX;
    }

    public Double getPositionY() {
        return positionY;
    }

    public double getTempMin() {
        return tempMin;
    }

    public double getTempMax() {
        return tempMax;
    }

    public double getHumiMin() {
        return humiMin;
    }

    public double getHumiMax() {
        return humiMax;
    }

    public int getSoilMin() {
        return soilMin;
    }

    public int getSoilMax() {
        return soilMax;
    }

    public int getLightMin() {
        return lightMin;
    }

    public int getLightMax() {
        return lightMax;
    }

    public String getCreatedAt() {
        return createdAt;
    }
}
