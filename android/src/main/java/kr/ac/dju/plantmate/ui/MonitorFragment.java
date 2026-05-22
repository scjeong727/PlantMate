package kr.ac.dju.plantmate.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

import kr.ac.dju.plantmate.MainActivity;
import kr.ac.dju.plantmate.R;
import kr.ac.dju.plantmate.model.MonitorSnapshot;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.service.PlantClientService;
import kr.ac.dju.plantmate.ui.adapter.TextLineAdapter;

public class MonitorFragment extends Fragment implements MainActivity.ServiceAwareFragment {

    private static final String SENSOR_DEVICE_EMPTY_HINT = "센서 모터를 연결해주세요";

    private final List<PlantProfile> plantList = new ArrayList<>();
    private ArrayAdapter<String> plantAdapter;
    private ArrayAdapter<String> sensorDeviceAdapter;
    private TextLineAdapter sensorHistoryAdapter;
    private TextLineAdapter eventHistoryAdapter;

    private Spinner spinnerPlant;
    private Spinner spinnerSensorDevice;
    private TextView textTemp;
    private TextView textHumi;
    private TextView textSoil;
    private TextView textLight;
    private TextView textRecentEvent;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_monitor, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        spinnerPlant = view.findViewById(R.id.spinner_monitor_plant);
        spinnerSensorDevice = view.findViewById(R.id.spinner_sensor_device);
        textTemp = view.findViewById(R.id.text_temp_value);
        textHumi = view.findViewById(R.id.text_humi_value);
        textSoil = view.findViewById(R.id.text_soil_value);
        textLight = view.findViewById(R.id.text_light_value);
        textRecentEvent = view.findViewById(R.id.text_recent_event);

        plantAdapter = new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_item, new ArrayList<>());
        plantAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerPlant.setAdapter(plantAdapter);

        sensorDeviceAdapter = new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_item, new ArrayList<>());
        sensorDeviceAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerSensorDevice.setAdapter(sensorDeviceAdapter);

        RecyclerView sensorRecycler = view.findViewById(R.id.recycler_sensor_history);
        sensorRecycler.setLayoutManager(new LinearLayoutManager(requireContext()));
        sensorHistoryAdapter = new TextLineAdapter();
        sensorRecycler.setAdapter(sensorHistoryAdapter);

        RecyclerView eventRecycler = view.findViewById(R.id.recycler_event_history);
        eventRecycler.setLayoutManager(new LinearLayoutManager(requireContext()));
        eventHistoryAdapter = new TextLineAdapter();
        eventRecycler.setAdapter(eventHistoryAdapter);

        ((Button) view.findViewById(R.id.btn_monitor_refresh)).setOnClickListener(v -> loadMonitor());
        ((Button) view.findViewById(R.id.btn_sensor_stream_start)).setOnClickListener(v -> startSensorStream());
        ((Button) view.findViewById(R.id.btn_sensor_stream_stop)).setOnClickListener(v -> stopSensorStream());
    }

    @Override
    public void onServiceReady() {
        loadPlants();
        loadSensorDevices();
    }

    private void loadPlants() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        service.loadPlants(new PlantClientService.ServiceCallback<List<PlantProfile>>() {
            @Override
            public void onSuccess(List<PlantProfile> data) {
                plantList.clear();
                plantList.addAll(data);
                plantAdapter.clear();
                for (PlantProfile plant : plantList) {
                    plantAdapter.add(plant.getPlantId() + " - " + plant.getName());
                }
                plantAdapter.notifyDataSetChanged();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void loadSensorDevices() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        service.loadSensorDevices(new PlantClientService.ServiceCallback<List<String>>() {
            @Override
            public void onSuccess(List<String> data) {
                sensorDeviceAdapter.clear();
                if (data == null || data.isEmpty()) {
                    sensorDeviceAdapter.add(SENSOR_DEVICE_EMPTY_HINT);
                } else {
                    sensorDeviceAdapter.addAll(data);
                }
                sensorDeviceAdapter.notifyDataSetChanged();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void loadMonitor() {
        PlantClientService service = getService();
        PlantProfile plant = getSelectedPlant();
        if (service == null || plant == null) {
            return;
        }

        service.loadMonitorSnapshot(plant.getPlantId(), new PlantClientService.ServiceCallback<MonitorSnapshot>() {
            @Override
            public void onSuccess(MonitorSnapshot data) {
                bindRecentSensor(data.getRecentSensorText());
                textRecentEvent.setText(data.getRecentEventText());
                sensorHistoryAdapter.submitLines(splitLines(data.getSensorHistoryText()));
                eventHistoryAdapter.submitLines(splitLines(data.getEventHistoryText()));
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void startSensorStream() {
        PlantClientService service = getService();
        PlantProfile plant = getSelectedPlant();
        String path = (String) spinnerSensorDevice.getSelectedItem();
        if (service == null || plant == null) {
            return;
        }
        if (path == null || path.isEmpty() || SENSOR_DEVICE_EMPTY_HINT.equals(path)) {
            showToast(SENSOR_DEVICE_EMPTY_HINT);
            return;
        }

        service.setSensorDevice(path, plant.getPlantId(), new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showToast(data);
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void stopSensorStream() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        service.stopSensorStream(new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showToast(data);
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void bindRecentSensor(String sensorText) {
        String[] tokens = sensorText.split(" ");
        textTemp.setText(tokens.length > 0 ? tokens[0].replace("T:", "") : "-");
        textHumi.setText(tokens.length > 1 ? tokens[1].replace("H:", "") : "-");
        textSoil.setText(tokens.length > 2 ? tokens[2].replace("S:", "") : "-");
        textLight.setText(tokens.length > 3 ? tokens[3].replace("L:", "") : "-");
    }

    private PlantProfile getSelectedPlant() {
        int position = spinnerPlant.getSelectedItemPosition();
        if (position < 0 || position >= plantList.size()) {
            showToast("식물을 선택하세요.");
            return null;
        }
        return plantList.get(position);
    }

    private List<String> splitLines(String value) {
        List<String> result = new ArrayList<>();
        if (value == null || value.trim().isEmpty()) {
            return result;
        }
        String[] lines = value.split("\n");
        for (String line : lines) {
            result.add(line);
        }
        return result;
    }

    private PlantClientService getService() {
        if (!(requireActivity() instanceof MainActivity)) {
            return null;
        }
        return ((MainActivity) requireActivity()).getPlantService();
    }

    private void showToast(String message) {
        if (requireActivity() instanceof MainActivity) {
            ((MainActivity) requireActivity()).showToast(message);
        }
    }
}
