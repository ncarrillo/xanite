package com.example.xemu;

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.util.ArrayList;
import java.util.List;

public class GameAdapter extends RecyclerView.Adapter<GameAdapter.GameViewHolder> implements Filterable {

    private final List<String> gameNamesList;
    private final List<String> filteredGameNamesList;
    private final OnGameClickListener onGameClickListener;
    private final Context context;

    public GameAdapter(Context context, List<String> gameNamesList, OnGameClickListener onGameClickListener) {
        this.context = context;
        this.gameNamesList = gameNamesList;
        this.filteredGameNamesList = new ArrayList<>(gameNamesList);
        this.onGameClickListener = onGameClickListener;
    }

    @NonNull
    @Override
    public GameViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_game, parent, false);
        return new GameViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull GameViewHolder holder, int position) {
        String gameName = filteredGameNamesList.get(position);
        holder.gameNameTextView.setText(gameName);


holder.gameImageView.setImageResource(R.drawable.placeholder_image);


        holder.itemView.setOnClickListener(v -> onGameClickListener.onGameSelected(gameName));
    }

    @Override
    public int getItemCount() {
        return filteredGameNamesList.size();
    }

    @Override
    public Filter getFilter() {
        return new Filter() {
            @Override
            protected FilterResults performFiltering(CharSequence constraint) {
                FilterResults results = new FilterResults();
                List<String> filteredList = new ArrayList<>();

                if (constraint == null || constraint.length() == 0) {
                    filteredList.addAll(gameNamesList);
                } else {
                    String filterPattern = constraint.toString().toLowerCase().trim();
                    for (String gameName : gameNamesList) {
                        if (gameName.toLowerCase().contains(filterPattern)) {
                            filteredList.add(gameName);
                        }
                    }
                }

                results.values = filteredList;
                results.count = filteredList.size();
                return results;
            }

            @Override
            protected void publishResults(CharSequence constraint, FilterResults results) {
                filteredGameNamesList.clear();
                filteredGameNamesList.addAll((List) results.values);
                notifyDataSetChanged();
            }
        };
    }

    public static class GameViewHolder extends RecyclerView.ViewHolder {
        public TextView gameNameTextView;
        public ImageView gameImageView;

        public GameViewHolder(@NonNull View itemView) {
            super(itemView);
            gameNameTextView = itemView.findViewById(R.id.game_name_text_view);
            gameImageView = itemView.findViewById(R.id.game_image_view); 
        }
    }

    public interface OnGameClickListener {
        void onGameSelected(String gameName);
    }
}