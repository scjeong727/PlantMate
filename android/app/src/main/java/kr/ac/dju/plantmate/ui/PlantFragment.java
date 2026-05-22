package kr.ac.dju.plantmate.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import com.google.android.material.bottomnavigation.BottomNavigationView;

import kr.ac.dju.plantmate.MainActivity;
import kr.ac.dju.plantmate.R;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.service.PlantClientService;
import kr.ac.dju.plantmate.ui.adapter.PlantAdapter;

public class PlantFragment extends Fragment implements MainActivity.ServiceAwareFragment {

    private PlantAdapter adapter;
    private RecyclerView recyclerView;
    private View detailPanel;
    private View infoPanel;
    private View waterPanel;
    private View robotPanel;
    private View logPanel;
    private ArrayAdapter<String> robotDeviceAdapter;
    private EditText editWaterDuration;
    private Spinner spinnerRobotDevice;
    private EditText editRobotLinear;
    private EditText editRobotAngular;
    private EditText editPositionX;
    private EditText editPositionY;
    private EditText editTempMin;
    private EditText editTempMax;
    private EditText editHumiMin;
    private EditText editHumiMax;
    private EditText editLightMin;
    private EditText editLightMax;
    private EditText editSoilMin;
    private EditText editSoilMax;
    private EditText textDetailName;
    private EditText textDetailType;
    private TextView textDetailLog;
    private TextView textRobotStatus;
    private BottomNavigationView detailBottomNavigation;
    private PlantProfile selectedPlant;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_plant, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        recyclerView = view.findViewById(R.id.recycler_plants);
        detailPanel = view.findViewById(R.id.plant_detail_panel);
        infoPanel = view.findViewById(R.id.panel_detail_info);
        waterPanel = view.findViewById(R.id.panel_detail_water);
        robotPanel = view.findViewById(R.id.panel_detail_robot);
        logPanel = view.findViewById(R.id.panel_detail_log);
        editWaterDuration = view.findViewById(R.id.edit_detail_water_duration);
        spinnerRobotDevice = view.findViewById(R.id.spinner_detail_robot_device);
        editRobotLinear = view.findViewById(R.id.edit_detail_robot_linear);
        editRobotAngular = view.findViewById(R.id.edit_detail_robot_angular);
        editPositionX = view.findViewById(R.id.edit_detail_position_x);
        editPositionY = view.findViewById(R.id.edit_detail_position_y);
        editTempMin = view.findViewById(R.id.edit_detail_temp_min);
        editTempMax = view.findViewById(R.id.edit_detail_temp_max);
        editHumiMin = view.findViewById(R.id.edit_detail_humi_min);
        editHumiMax = view.findViewById(R.id.edit_detail_humi_max);
        editLightMin = view.findViewById(R.id.edit_detail_light_min);
        editLightMax = view.findViewById(R.id.edit_detail_light_max);
        editSoilMin = view.findViewById(R.id.edit_detail_soil_min);
        editSoilMax = view.findViewById(R.id.edit_detail_soil_max);
        textDetailName = view.findViewById(R.id.text_detail_name);
        textDetailType = view.findViewById(R.id.text_detail_type);
        textDetailLog = view.findViewById(R.id.text_detail_log);
        textRobotStatus = view.findViewById(R.id.text_detail_robot_status);
        detailBottomNavigation = view.findViewById(R.id.detail_bottom_navigation);

        robotDeviceAdapter = new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_item, new ArrayList<>());
        robotDeviceAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerRobotDevice.setAdapter(robotDeviceAdapter);

        recyclerView.setLayoutManager(new GridLayoutManager(requireContext(), 3));
        adapter = new PlantAdapter(this::showPlantDetail);
        recyclerView.setAdapter(adapter);

        view.findViewById(R.id.btn_plant_detail_back).setOnClickListener(v -> showPlantList());
        ((Button) view.findViewById(R.id.btn_detail_water_run)).setOnClickListener(v -> runWater());
        ((Button) view.findViewById(R.id.btn_detail_robot_refresh)).setOnClickListener(v -> loadRobotDevices());
        ((Button) view.findViewById(R.id.btn_detail_robot_bind)).setOnClickListener(v -> bindRobotDevice());
        ((Button) view.findViewById(R.id.btn_detail_robot_move)).setOnClickListener(v -> sendRobotMove());
        ((Button) view.findViewById(R.id.btn_detail_robot_stop)).setOnClickListener(v -> sendRobotStop());
        editPositionX.setOnFocusChangeListener((v, hasFocus) -> {
            if (!hasFocus) {
                savePositionIfChanged();
            }
        });
        editPositionY.setOnFocusChangeListener((v, hasFocus) -> {
            if (!hasFocus) {
                savePositionIfChanged();
            }
        });
        setCriteriaAutoSave(editTempMin);
        setCriteriaAutoSave(editTempMax);
        setCriteriaAutoSave(editHumiMin);
        setCriteriaAutoSave(editHumiMax);
        setCriteriaAutoSave(editLightMin);
        setCriteriaAutoSave(editLightMax);
        setCriteriaAutoSave(editSoilMin);
        setCriteriaAutoSave(editSoilMax);
        setNameTypeAutoSave(textDetailName);
        setNameTypeAutoSave(textDetailType);
        detailBottomNavigation.setOnItemSelectedListener(item -> {
            if (item.getItemId() == R.id.menu_detail_info) {
                showDetailTab(infoPanel);
                return true;
            }
            if (item.getItemId() == R.id.menu_detail_water) {
                showDetailTab(waterPanel);
                return true;
            }
            if (item.getItemId() == R.id.menu_detail_robot) {
                showDetailTab(robotPanel);
                loadRobotDevices();
                return true;
            }
            if (item.getItemId() == R.id.menu_detail_log) {
                showDetailTab(logPanel);
                loadWaterHistory();
                return true;
            }
            return false;
        });
    }

    @Override
    public void onServiceReady() {
        loadPlants();
    }

    private void loadPlants() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        service.loadPlants(new PlantClientService.ServiceCallback<List<PlantProfile>>() {
            @Override
            public void onSuccess(List<PlantProfile> data) {
                adapter.submitList(new ArrayList<>(data));
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void showPlantDetail(PlantProfile plant) {
        selectedPlant = plant;
        recyclerView.setVisibility(View.GONE);
        detailPanel.setVisibility(View.VISIBLE);

        bindNameType(plant);
        bindPosition(plant);
        bindCriteria(plant);
        textDetailLog.setText("");
        textRobotStatus.setText("");
        detailBottomNavigation.setSelectedItemId(R.id.menu_detail_info);
        showDetailTab(infoPanel);
    }

    private void showPlantList() {
        selectedPlant = null;
        detailPanel.setVisibility(View.GONE);
        recyclerView.setVisibility(View.VISIBLE);
    }

    private void showDetailTab(View selectedPanel) {
        infoPanel.setVisibility(selectedPanel == infoPanel ? View.VISIBLE : View.GONE);
        waterPanel.setVisibility(selectedPanel == waterPanel ? View.VISIBLE : View.GONE);
        robotPanel.setVisibility(selectedPanel == robotPanel ? View.VISIBLE : View.GONE);
        logPanel.setVisibility(selectedPanel == logPanel ? View.VISIBLE : View.GONE);
    }

    private void runWater() {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        int duration;
        try {
            duration = Integer.parseInt(editWaterDuration.getText().toString().trim());
        } catch (NumberFormatException e) {
            showToast("급수 시간을 확인하세요.");
            return;
        }

        service.waterPlant(selectedPlant.getPlantId(), duration, new PlantClientService.ServiceCallback<String>() {
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

    private void loadRobotDevices() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        service.loadRobotDevices(new PlantClientService.ServiceCallback<List<String>>() {
            @Override
            public void onSuccess(List<String> data) {
                robotDeviceAdapter.clear();
                if (data == null || data.isEmpty()) {
                    robotDeviceAdapter.add("온라인 로봇 없음");
                } else {
                    robotDeviceAdapter.addAll(data);
                }
                robotDeviceAdapter.notifyDataSetChanged();
            }

            @Override
            public void onError(String message) {
                textRobotStatus.setText(message);
                showToast(message);
            }
        });
    }

    private void bindRobotDevice() {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        String deviceId = (String) spinnerRobotDevice.getSelectedItem();
        if (deviceId == null || deviceId.isEmpty() || "온라인 로봇 없음".equals(deviceId)) {
            showToast("로봇 장치를 선택하세요.");
            return;
        }

        service.setRobotDevice(deviceId, selectedPlant.getPlantId(), new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                textRobotStatus.setText(deviceId + " 연결됨");
                showToast(data);
            }

            @Override
            public void onError(String message) {
                textRobotStatus.setText(message);
                showToast(message);
            }
        });
    }

    private void sendRobotMove() {
        float linear;
        float angular;
        try {
            linear = Float.parseFloat(editRobotLinear.getText().toString().trim());
            angular = Float.parseFloat(editRobotAngular.getText().toString().trim());
        } catch (NumberFormatException exception) {
            showToast("linear/angular 값을 확인하세요.");
            return;
        }

        sendRobotCommand("move", String.format(Locale.US, "linear=%.3f angular=%.3f", linear, angular));
    }

    private void sendRobotStop() {
        sendRobotCommand("move", "linear=0.000 angular=0.000");
    }

    private void sendRobotCommand(String action, String detail) {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        service.robotCommand(selectedPlant.getPlantId(), action, detail, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                textRobotStatus.setText(data + " | " + detail);
            }

            @Override
            public void onError(String message) {
                textRobotStatus.setText(message);
                showToast(message);
            }
        });
    }

    private void loadWaterHistory() {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        service.loadWaterHistory(selectedPlant.getPlantId(), new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                String log = data == null || data.trim().isEmpty() ? "로그가 없습니다." : data.trim();
                textDetailLog.setText(log);
            }

            @Override
            public void onError(String message) {
                textDetailLog.setText(message);
                showToast(message);
            }
        });
    }

    private void savePositionIfChanged() {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        Double positionX;
        Double positionY;
        try {
            positionX = parseNullableDouble(editPositionX);
            positionY = parseNullableDouble(editPositionY);
        } catch (IllegalArgumentException e) {
            showToast(e.getMessage());
            return;
        }

        if (sameNullableDouble(selectedPlant.getPositionX(), positionX)
                && sameNullableDouble(selectedPlant.getPositionY(), positionY)) {
            return;
        }

        PlantProfile updatedPlant = new PlantProfile(
                selectedPlant.getPlantId(),
                selectedPlant.getUserId(),
                readEditedName(),
                readEditedType(),
                positionX,
                positionY,
                selectedPlant.getTempMin(),
                selectedPlant.getTempMax(),
                selectedPlant.getHumiMin(),
                selectedPlant.getHumiMax(),
                selectedPlant.getSoilMin(),
                selectedPlant.getSoilMax(),
                selectedPlant.getLightMin(),
                selectedPlant.getLightMax(),
                selectedPlant.getCreatedAt()
        );

        service.editPlant(updatedPlant, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                selectedPlant = updatedPlant;
                bindPosition(updatedPlant);
                loadPlants();
                showToast(data);
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void saveNameTypeIfChanged() {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        String name = readEditedName();
        String type = readEditedType();
        if (name.isEmpty() || type.isEmpty()) {
            showToast("식물 이름과 종류를 입력하세요.");
            bindNameType(selectedPlant);
            return;
        }

        String currentName = selectedPlant.getName() != null ? selectedPlant.getName() : "";
        String currentType = selectedPlant.getType() != null ? selectedPlant.getType() : "";
        if (name.equals(currentName) && type.equals(currentType)) {
            return;
        }

        PlantProfile updatedPlant = new PlantProfile(
                selectedPlant.getPlantId(),
                selectedPlant.getUserId(),
                name,
                type,
                selectedPlant.getPositionX(),
                selectedPlant.getPositionY(),
                selectedPlant.getTempMin(),
                selectedPlant.getTempMax(),
                selectedPlant.getHumiMin(),
                selectedPlant.getHumiMax(),
                selectedPlant.getSoilMin(),
                selectedPlant.getSoilMax(),
                selectedPlant.getLightMin(),
                selectedPlant.getLightMax(),
                selectedPlant.getCreatedAt()
        );

        service.editPlant(updatedPlant, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                selectedPlant = updatedPlant;
                bindNameType(updatedPlant);
                loadPlants();
                showToast(data);
            }

            @Override
            public void onError(String message) {
                bindNameType(selectedPlant);
                showToast(message);
            }
        });
    }

    private void saveCriteriaIfChanged() {
        PlantClientService service = getService();
        if (service == null || selectedPlant == null) {
            return;
        }

        double tempMin;
        double tempMax;
        double humiMin;
        double humiMax;
        int lightMin;
        int lightMax;
        int soilMin;
        int soilMax;
        try {
            tempMin = parseDouble(editTempMin);
            tempMax = parseDouble(editTempMax);
            humiMin = parseDouble(editHumiMin);
            humiMax = parseDouble(editHumiMax);
            lightMin = parseInt(editLightMin);
            lightMax = parseInt(editLightMax);
            soilMin = parseInt(editSoilMin);
            soilMax = parseInt(editSoilMax);
        } catch (IllegalArgumentException e) {
            showToast(e.getMessage());
            return;
        }

        if (sameDouble(selectedPlant.getTempMin(), tempMin)
                && sameDouble(selectedPlant.getTempMax(), tempMax)
                && sameDouble(selectedPlant.getHumiMin(), humiMin)
                && sameDouble(selectedPlant.getHumiMax(), humiMax)
                && selectedPlant.getLightMin() == lightMin
                && selectedPlant.getLightMax() == lightMax
                && selectedPlant.getSoilMin() == soilMin
                && selectedPlant.getSoilMax() == soilMax) {
            return;
        }

        PlantProfile updatedPlant = new PlantProfile(
                selectedPlant.getPlantId(),
                selectedPlant.getUserId(),
                readEditedName(),
                readEditedType(),
                selectedPlant.getPositionX(),
                selectedPlant.getPositionY(),
                tempMin,
                tempMax,
                humiMin,
                humiMax,
                soilMin,
                soilMax,
                lightMin,
                lightMax,
                selectedPlant.getCreatedAt()
        );

        service.editPlant(updatedPlant, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                selectedPlant = updatedPlant;
                bindCriteria(updatedPlant);
                loadPlants();
                showToast(data);
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void bindNameType(PlantProfile plant) {
        textDetailName.setText(plant.getName() != null ? plant.getName() : "");
        textDetailType.setText(plant.getType() != null ? plant.getType() : "");
    }

    private String readEditedName() {
        return textDetailName.getText().toString().trim();
    }

    private String readEditedType() {
        return textDetailType.getText().toString().trim();
    }

    private void bindPosition(PlantProfile plant) {
        editPositionX.setText(formatNullableDouble(plant.getPositionX()));
        editPositionY.setText(formatNullableDouble(plant.getPositionY()));
    }

    private void bindCriteria(PlantProfile plant) {
        editTempMin.setText(formatDouble(plant.getTempMin()));
        editTempMax.setText(formatDouble(plant.getTempMax()));
        editHumiMin.setText(formatDouble(plant.getHumiMin()));
        editHumiMax.setText(formatDouble(plant.getHumiMax()));
        editLightMin.setText(String.valueOf(plant.getLightMin()));
        editLightMax.setText(String.valueOf(plant.getLightMax()));
        editSoilMin.setText(String.valueOf(plant.getSoilMin()));
        editSoilMax.setText(String.valueOf(plant.getSoilMax()));
    }

    private Double parseNullableDouble(EditText editText) {
        String value = editText.getText().toString().trim();
        if (value.isEmpty()) {
            return null;
        }
        try {
            return Double.parseDouble(value);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException(editText.getHint() + " 값을 확인하세요.");
        }
    }

    private String formatNullableDouble(Double value) {
        if (value == null) {
            return "";
        }
        return formatDouble(value);
    }

    private String formatDouble(double value) {
        return String.format(Locale.getDefault(), "%.2f", value);
    }

    private double parseDouble(EditText editText) {
        String value = editText.getText().toString().trim();
        if (value.isEmpty()) {
            return 0.0;
        }
        try {
            return Double.parseDouble(value);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException(editText.getHint() + " 값을 확인하세요.");
        }
    }

    private int parseInt(EditText editText) {
        String value = editText.getText().toString().trim();
        if (value.isEmpty()) {
            return 0;
        }
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException(editText.getHint() + " 값을 확인하세요.");
        }
    }

    private boolean sameNullableDouble(Double first, Double second) {
        if (first == null || second == null) {
            return first == null && second == null;
        }
        return Math.abs(first - second) < 0.000001;
    }

    private boolean sameDouble(double first, double second) {
        return Math.abs(first - second) < 0.000001;
    }

    private void setCriteriaAutoSave(EditText editText) {
        editText.setOnFocusChangeListener((v, hasFocus) -> {
            if (!hasFocus) {
                saveCriteriaIfChanged();
            }
        });
    }

    private void setNameTypeAutoSave(EditText editText) {
        editText.setOnFocusChangeListener((v, hasFocus) -> {
            if (!hasFocus) {
                saveNameTypeIfChanged();
            }
        });
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
