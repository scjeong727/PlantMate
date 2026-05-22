package kr.ac.dju.plantmate.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

import kr.ac.dju.plantmate.MainActivity;
import kr.ac.dju.plantmate.R;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.service.PlantClientService;
import kr.ac.dju.plantmate.ui.adapter.TextLineAdapter;

public class WaterFragment extends Fragment implements MainActivity.ServiceAwareFragment {

    private static final String WATER_DEVICE_EMPTY_HINT = "센서 모터를 연결해주세요";

    private final List<PlantProfile> plantList = new ArrayList<>();
    private ArrayAdapter<String> plantAdapter;
    private ArrayAdapter<String> waterDeviceAdapter;
    private TextLineAdapter waterHistoryAdapter;

    private Spinner spinnerPlant;
    private Spinner spinnerWaterDevice;
    private EditText editWaterDuration;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_water, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        spinnerPlant = view.findViewById(R.id.spinner_water_plant);
        spinnerWaterDevice = view.findViewById(R.id.spinner_water_device);
        editWaterDuration = view.findViewById(R.id.edit_water_duration);

        plantAdapter = new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_item, new ArrayList<>());
        plantAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerPlant.setAdapter(plantAdapter);

        waterDeviceAdapter = new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_item, new ArrayList<>());
        waterDeviceAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerWaterDevice.setAdapter(waterDeviceAdapter);

        RecyclerView recyclerView = view.findViewById(R.id.recycler_water_history);
        recyclerView.setLayoutManager(new LinearLayoutManager(requireContext()));
        waterHistoryAdapter = new TextLineAdapter();
        recyclerView.setAdapter(waterHistoryAdapter);

        ((Button) view.findViewById(R.id.btn_water_device_load)).setOnClickListener(v -> loadWaterDevices());
        ((Button) view.findViewById(R.id.btn_water_device_set)).setOnClickListener(v -> setWaterDevice());
        ((Button) view.findViewById(R.id.btn_water_run)).setOnClickListener(v -> runWater());
        ((Button) view.findViewById(R.id.btn_water_history_refresh)).setOnClickListener(v -> loadWaterHistory());
    }

    @Override
    public void onServiceReady() {
        loadPlants();
        loadWaterDevices();
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
                    plantAdapter.add(plant.getName());
                }
                plantAdapter.notifyDataSetChanged();
                loadWaterHistory();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void loadWaterDevices() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        service.loadWaterDevices(new PlantClientService.ServiceCallback<List<String>>() {
            @Override
            public void onSuccess(List<String> data) {
                waterDeviceAdapter.clear();
                if (data == null || data.isEmpty()) {
                    waterDeviceAdapter.add(WATER_DEVICE_EMPTY_HINT);
                } else {
                    waterDeviceAdapter.addAll(data);
                }
                waterDeviceAdapter.notifyDataSetChanged();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void setWaterDevice() {
        PlantClientService service = getService();
        PlantProfile plant = getSelectedPlant();
        if (service == null || plant == null) {
            return;
        }

        String path = (String) spinnerWaterDevice.getSelectedItem();
        if (path == null || path.isEmpty() || WATER_DEVICE_EMPTY_HINT.equals(path)) {
            showToast(WATER_DEVICE_EMPTY_HINT);
            return;
        }

        service.setWaterDevice(path, plant.getPlantId(), new PlantClientService.ServiceCallback<String>() {
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

    private void runWater() {
        PlantClientService service = getService();
        PlantProfile plant = getSelectedPlant();
        if (service == null || plant == null) {
            return;
        }

        int duration;
        try {
            duration = Integer.parseInt(editWaterDuration.getText().toString().trim());
        } catch (NumberFormatException e) {
            showToast("급수 시간을 확인하세요.");
            return;
        }

        service.waterPlant(plant.getPlantId(), duration, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showToast(data);
                loadWaterHistory();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void loadWaterHistory() {
        PlantClientService service = getService();
        PlantProfile plant = getSelectedPlant();
        if (service == null || plant == null) {
            return;
        }

        service.loadWaterHistory(plant.getPlantId(), new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                waterHistoryAdapter.submitLines(splitLines(data));
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private PlantProfile getSelectedPlant() {
        int position = spinnerPlant.getSelectedItemPosition();
        if (position < 0 || position >= plantList.size()) {
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
