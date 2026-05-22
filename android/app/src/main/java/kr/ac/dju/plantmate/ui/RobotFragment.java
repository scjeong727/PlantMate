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

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import kr.ac.dju.plantmate.MainActivity;
import kr.ac.dju.plantmate.R;
import kr.ac.dju.plantmate.model.PlantProfile;
import kr.ac.dju.plantmate.service.PlantClientService;

public class RobotFragment extends Fragment implements MainActivity.ServiceAwareFragment {

    private final List<PlantProfile> plantList = new ArrayList<>();
    private ArrayAdapter<String> plantAdapter;

    private Spinner spinnerPlant;
    private TextView textStatus;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_robot, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        spinnerPlant = view.findViewById(R.id.spinner_robot_plant);
        textStatus = view.findViewById(R.id.text_robot_status);

        plantAdapter = new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_item, new ArrayList<>());
        plantAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerPlant.setAdapter(plantAdapter);

        Button moveButton = view.findViewById(R.id.btn_robot_move);
        moveButton.setEnabled(false);
        moveButton.setOnClickListener(v -> sendMove());
        ((Button) view.findViewById(R.id.btn_robot_stop)).setOnClickListener(v -> sendStop());
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
                plantAdapter.clear();
                for (PlantProfile plant : plantList) {
                    plantAdapter.add(plant.getName());
                }
                plantAdapter.notifyDataSetChanged();
            }

            @Override
            public void onError(String message) {
                showStatus(message);
            }
        });
    }

    private void sendMove() {
        PlantProfile plant = getSelectedPlant();
        if (plant == null) {
            showStatus("식물을 선택하세요.");
            return;
        }

        String detail = String.format(
                Locale.US,
                "x=%.3f y=%.3f",
                1.0,
                0.0
        );
        sendRobotCommand("move", detail);
    }

    private void sendStop() {
        showStatus("정지는 현재 좌표 이동 방식에서 사용하지 않습니다.");
    }

    private void sendRobotCommand(String action, String detail) {
        PlantClientService service = getService();
        PlantProfile plant = getSelectedPlant();
        if (service == null || plant == null) {
            showStatus("식물을 선택하세요.");
            return;
        }

        service.robotCommand(plant.getPlantId(), action, detail, new PlantClientService.ServiceCallback<String>() {
            @Override
            public void onSuccess(String data) {
                showStatus(data + " | " + detail);
            }

            @Override
            public void onError(String message) {
                showStatus(message);
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

    private PlantClientService getService() {
        MainActivity activity = (MainActivity) getActivity();
        if (activity == null) {
            return null;
        }
        return activity.getPlantService();
    }

    private void showStatus(String message) {
        if (textStatus != null) {
            textStatus.setText(message);
        }
        MainActivity activity = (MainActivity) getActivity();
        if (activity != null) {
            activity.showToast(message);
        }
    }
}
