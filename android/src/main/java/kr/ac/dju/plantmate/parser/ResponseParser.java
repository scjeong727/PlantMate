package kr.ac.dju.plantmate.parser;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

import kr.ac.dju.plantmate.model.EventRecord;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.model.SensorRecord;

public class ResponseParser {

    public void requireOk(String response) {
        if (response == null || !response.startsWith("OK")) {
            throw new IllegalStateException(response == null ? "빈 응답" : response);
        }
    }

    public JSONObject parseObject(String response) {
        int index = findPayloadStart(response, '{');
        if (index < 0) {
            throw new IllegalStateException("객체 응답 파싱 실패");
        }

        try {
            return new JSONObject(response.substring(index));
        } catch (JSONException e) {
            throw new IllegalStateException("객체 응답 파싱 실패", e);
        }
    }

    public JSONArray parseArray(String response) {
        int index = findPayloadStart(response, '[');
        if (index < 0) {
            throw new IllegalStateException("배열 응답 파싱 실패");
        }

        try {
            return new JSONArray(response.substring(index));
        } catch (JSONException e) {
            throw new IllegalStateException("배열 응답 파싱 실패", e);
        }
    }

    public int parseUserId(String response) {
        JSONObject object = parseObject(response);
        return object.optInt("user_id", -1);
    }

    public List<PlantProfile> parsePlants(String response) {
        JSONArray array = parseArray(response);
        List<PlantProfile> result = new ArrayList<>();

        for (int i = 0; i < array.length(); i++) {
            JSONObject item = array.optJSONObject(i);
            if (item == null) {
                continue;
            }

            int plantId = item.optInt("plant_id", item.optInt("id", -1));
            result.add(new PlantProfile(
                    plantId,
                    item.optInt("user_id", -1),
                    item.optString("name", ""),
                    item.optString("type", ""),
                    readNullableDouble(item, "position_x"),
                    readNullableDouble(item, "position_y"),
                    item.optDouble("temp_min", 0.0),
                    item.optDouble("temp_max", 0.0),
                    item.optDouble("humi_min", 0.0),
                    item.optDouble("humi_max", 0.0),
                    item.optInt("soil_min", 0),
                    item.optInt("soil_max", 0),
                    item.optInt("light_min", 0),
                    item.optInt("light_max", 0),
                    item.optString("created_at", "")
            ));
        }

        return result;
    }

    public SensorRecord parseRecentSensor(String response) {
        JSONObject object = parseObject(response);
        return new SensorRecord(
                object.optString("created_at", ""),
                object.optString("temp", ""),
                object.optString("humi", ""),
                object.optString("soil", ""),
                object.optString("light", "")
        );
    }

    public EventRecord parseRecentEvent(String response) {
        JSONObject object = parseObject(response);
        return new EventRecord(
                object.optString("created_at", ""),
                object.optString("event_type", ""),
                object.optString("message", ""),
                object.optString("plant_id", "")
        );
    }

    public List<SensorRecord> parseSensorHistory(String response) {
        JSONArray array = parseArray(response);
        List<SensorRecord> result = new ArrayList<>();

        for (int i = 0; i < array.length(); i++) {
            JSONObject item = array.optJSONObject(i);
            if (item == null) {
                continue;
            }
            result.add(new SensorRecord(
                    item.optString("created_at", ""),
                    item.optString("temp", ""),
                    item.optString("humi", ""),
                    item.optString("soil", ""),
                    item.optString("light", "")
            ));
        }

        return result;
    }

    public List<EventRecord> parseEventHistory(String response) {
        JSONArray array = parseArray(response);
        List<EventRecord> result = new ArrayList<>();

        for (int i = 0; i < array.length(); i++) {
            JSONObject item = array.optJSONObject(i);
            if (item == null) {
                continue;
            }
            result.add(new EventRecord(
                    item.optString("created_at", ""),
                    item.optString("event_type", ""),
                    item.optString("message", ""),
                    item.optString("plant_id", "")
            ));
        }

        return result;
    }

    public List<String> parseDeviceList(String response) {
        JSONArray array = parseArray(response);
        List<String> result = new ArrayList<>();

        for (int i = 0; i < array.length(); i++) {
            String item = array.optString(i, "");
            if (!item.isEmpty()) {
                result.add(item);
            }
        }

        return result;
    }

    private int findPayloadStart(String response, char token) {
        if (response == null || response.trim().isEmpty()) {
            return -1;
        }

        String trimmed = response.trim();
        if (trimmed.startsWith("OK")) {
            return trimmed.indexOf(token);
        }

        return trimmed.charAt(0) == token ? 0 : trimmed.indexOf(token);
    }

    private Double readNullableDouble(JSONObject object, String key) {
        if (!object.has(key) || object.isNull(key)) {
            return null;
        }
        return object.optDouble(key, 0.0);
    }
}
