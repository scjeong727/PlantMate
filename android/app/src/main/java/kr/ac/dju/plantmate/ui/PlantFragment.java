package kr.ac.dju.plantmate.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import kr.ac.dju.plantmate.MainActivity;
import kr.ac.dju.plantmate.R;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.service.PlantClientService;
import kr.ac.dju.plantmate.ui.adapter.PlantAdapter;

public class PlantFragment extends Fragment implements MainActivity.ServiceAwareFragment {

    private final List<PlantProfile> plantList = new ArrayList<>();
    private PlantAdapter adapter;
    private int selectedPlantId = -1;

    private EditText editPlantName;
    private EditText editPlantType;
    private EditText editTempMin;
    private EditText editTempMax;
    private EditText editHumiMin;
    private EditText editHumiMax;
    private EditText editSoilMin;
    private EditText editSoilMax;
    private EditText editLightMin;
    private EditText editLightMax;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_plant, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        editPlantName = view.findViewById(R.id.edit_plant_name);
        editPlantType = view.findViewById(R.id.edit_plant_type);
        editTempMin = view.findViewById(R.id.edit_temp_min);
        editTempMax = view.findViewById(R.id.edit_temp_max);
        editHumiMin = view.findViewById(R.id.edit_humi_min);
        editHumiMax = view.findViewById(R.id.edit_humi_max);
        editSoilMin = view.findViewById(R.id.edit_soil_min);
        editSoilMax = view.findViewById(R.id.edit_soil_max);
        editLightMin = view.findViewById(R.id.edit_light_min);
        editLightMax = view.findViewById(R.id.edit_light_max);

        RecyclerView recyclerView = view.findViewById(R.id.recycler_plants);
        recyclerView.setLayoutManager(new LinearLayoutManager(requireContext()));
        adapter = new PlantAdapter(plant -> bindPlantForm(plant));
        recyclerView.setAdapter(adapter);

        ((Button) view.findViewById(R.id.btn_plant_load)).setOnClickListener(v -> loadPlants());
        ((Button) view.findViewById(R.id.btn_plant_add)).setOnClickListener(v -> addPlant());
        ((Button) view.findViewById(R.id.btn_plant_edit)).setOnClickListener(v -> editPlant());
        ((Button) view.findViewById(R.id.btn_plant_delete)).setOnClickListener(v -> deletePlant());
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
                plantList.clear();
                plantList.addAll(data);
                adapter.submitList(new ArrayList<>(plantList));
                if (!plantList.isEmpty()) {
                    PlantProfile selectedPlant = findPlantById(selectedPlantId);
                    bindPlantForm(selectedPlant != null ? selectedPlant : plantList.get(0));
                }
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void addPlant() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        PlantProfile form;
        try {
            form = readPlantForm(-1);
        } catch (IllegalArgumentException e) {
            showToast(e.getMessage());
            return;
        }

        service.addPlant(form, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showToast(data);
                loadPlants();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void editPlant() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        if (selectedPlantId <= 0) {
            showToast("수정할 식물을 먼저 선택하세요.");
            return;
        }

        PlantProfile form;
        try {
            form = readPlantForm(selectedPlantId);
        } catch (IllegalArgumentException e) {
            showToast(e.getMessage());
            return;
        }

        service.editPlant(form, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showToast(data);
                loadPlants();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void deletePlant() {
        PlantClientService service = getService();
        if (service == null) {
            return;
        }

        if (selectedPlantId <= 0) {
            showToast("삭제할 식물을 먼저 선택하세요.");
            return;
        }

        final int plantId = selectedPlantId;
        service.deletePlant(plantId, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showToast(data);
                selectedPlantId = -1;
                clearPlantForm();
                loadPlants();
            }

            @Override
            public void onError(String message) {
                showToast(message);
            }
        });
    }

    private void bindPlantForm(PlantProfile plant) {
        selectedPlantId = plant.getPlantId();
        adapter.setSelectedPlantId(selectedPlantId);
        editPlantName.setText(plant.getName());
        editPlantType.setText(plant.getType());
        editTempMin.setText(String.format(Locale.getDefault(), "%.2f", plant.getTempMin()));
        editTempMax.setText(String.format(Locale.getDefault(), "%.2f", plant.getTempMax()));
        editHumiMin.setText(String.format(Locale.getDefault(), "%.2f", plant.getHumiMin()));
        editHumiMax.setText(String.format(Locale.getDefault(), "%.2f", plant.getHumiMax()));
        editSoilMin.setText(String.valueOf(plant.getSoilMin()));
        editSoilMax.setText(String.valueOf(plant.getSoilMax()));
        editLightMin.setText(String.valueOf(plant.getLightMin()));
        editLightMax.setText(String.valueOf(plant.getLightMax()));
    }

    private PlantProfile findPlantById(int plantId) {
        for (PlantProfile plant : plantList) {
            if (plant.getPlantId() == plantId) {
                return plant;
            }
        }
        return null;
    }

    private PlantProfile readPlantForm(int plantId) {
        return new PlantProfile(
                plantId,
                0,
                getText(editPlantName),
                getText(editPlantType),
                parseDouble(editTempMin),
                parseDouble(editTempMax),
                parseDouble(editHumiMin),
                parseDouble(editHumiMax),
                parseInt(editSoilMin),
                parseInt(editSoilMax),
                parseInt(editLightMin),
                parseInt(editLightMax),
                ""
        );
    }

    private void clearPlantForm() {
        editPlantName.setText("");
        editPlantType.setText("");
        editTempMin.setText("");
        editTempMax.setText("");
        editHumiMin.setText("");
        editHumiMax.setText("");
        editSoilMin.setText("");
        editSoilMax.setText("");
        editLightMin.setText("");
        editLightMax.setText("");
    }

    private double parseDouble(EditText editText) {
        String value = getText(editText);
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
        String value = getText(editText);
        if (value.isEmpty()) {
            return 0;
        }
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException(editText.getHint() + " 값을 확인하세요.");
        }
    }

    private String getText(EditText editText) {
        return editText.getText().toString().trim();
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
