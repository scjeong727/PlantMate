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

public class TextLineAdapter extends RecyclerView.Adapter<TextLineAdapter.TextLineViewHolder> {

    private final List<String> items = new ArrayList<>();

    public void submitLines(List<String> lines) {
        items.clear();
        items.addAll(lines);
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public TextLineViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_text_line, parent, false);
        return new TextLineViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull TextLineViewHolder holder, int position) {
        holder.textLine.setText(items.get(position));
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    static class TextLineViewHolder extends RecyclerView.ViewHolder {
        private final TextView textLine;

        public TextLineViewHolder(@NonNull View itemView) {
            super(itemView);
            textLine = itemView.findViewById(R.id.text_line);
        }
    }
}
