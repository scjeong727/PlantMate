package kr.ac.dju.plantmate.ui.adapter;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
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
    private int selectedPlantId = -1;

    public PlantAdapter(OnPlantClickListener listener) {
        this.listener = listener;
    }

    public void submitList(List<PlantProfile> plants) {
        items.clear();
        items.addAll(plants);
        notifyDataSetChanged();
    }

    public void setSelectedPlantId(int plantId) {
        selectedPlantId = plantId;
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
        holder.textTitle.setText(plant.getPlantId() + " - " + plant.getName());
        holder.textSubtitle.setText(plant.getType());
        bindSelectionStyle(holder, plant.getPlantId() == selectedPlantId);
        holder.itemView.setOnClickListener(v -> listener.onPlantClick(plant));
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    private void bindSelectionStyle(@NonNull PlantViewHolder holder, boolean selected) {
        Context context = holder.itemView.getContext();
        if (selected) {
            holder.itemView.setBackgroundResource(R.drawable.bg_list_item_selected);
            holder.textTitle.setTextColor(ContextCompat.getColor(context, R.color.white));
            holder.textSubtitle.setTextColor(ContextCompat.getColor(context, R.color.surface_soft));
            return;
        }

        holder.itemView.setBackgroundResource(R.drawable.bg_list_item);
        holder.textTitle.setTextColor(ContextCompat.getColor(context, R.color.text_primary));
        holder.textSubtitle.setTextColor(ContextCompat.getColor(context, R.color.text_secondary));
    }

    static class PlantViewHolder extends RecyclerView.ViewHolder {
        private final TextView textTitle;
        private final TextView textSubtitle;

        public PlantViewHolder(@NonNull View itemView) {
            super(itemView);
            textTitle = itemView.findViewById(R.id.text_plant_title);
            textSubtitle = itemView.findViewById(R.id.text_plant_subtitle);
        }
    }
}
