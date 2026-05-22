package kr.ac.dju.plantmate.ui.adapter;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

import kr.ac.dju.plantmate.R;
import kr.ac.dju.plantmate.model.PlantProfile;

public class PlantAdapter extends RecyclerView.Adapter<PlantAdapter.PlantViewHolder> {

    public interface OnPlantClickListener {
        void onPlantClick(PlantProfile plant);
    }

    private final List<PlantProfile> items = new ArrayList<>();
    private final OnPlantClickListener listener;

    public PlantAdapter(OnPlantClickListener listener) {
        this.listener = listener;
    }

    public void submitList(List<PlantProfile> plants) {
        items.clear();
        items.addAll(plants);
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public PlantViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_plant, parent, false);
        return new PlantViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull PlantViewHolder holder, int position) {
        PlantProfile plant = items.get(position);
        holder.textName.setText(plant.getName());
        holder.itemView.setOnClickListener(v -> listener.onPlantClick(plant));
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    static class PlantViewHolder extends RecyclerView.ViewHolder {
        private final TextView textName;

        public PlantViewHolder(@NonNull View itemView) {
            super(itemView);
            textName = itemView.findViewById(R.id.text_plant_name);
        }
    }
}
