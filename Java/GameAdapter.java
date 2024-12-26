package com.example.xemu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class GameAdapter extends BaseAdapter implements Filterable {

    private Context context;
    private ArrayList<String> gameList;
    private ArrayList<String> filteredGameList;  // نسخة من القائمة لعرض العناصر المفلترة

    public GameAdapter(Context context, ArrayList<String> gameList) {
        this.context = context;
        this.gameList = gameList;
        this.filteredGameList = new ArrayList<>(gameList);  // قائمة مفلترة تحتوي على نفس العناصر في البداية
    }

    @Override
    public int getCount() {
        return filteredGameList.size();  // إرجاع حجم القائمة المفلترة
    }

    @Override
    public Object getItem(int position) {
        return filteredGameList.get(position);  // إرجاع العنصر المفلتر بناءً على الموضع
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            LayoutInflater inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            convertView = inflater.inflate(R.layout.item_game, null);
        }

        TextView gameNameTextView = convertView.findViewById(R.id.game_name_text);
        gameNameTextView.setText(filteredGameList.get(position));  // عرض العنصر من القائمة المفلترة

        return convertView;
    }

    @Override
    public Filter getFilter() {
        return new Filter() {
            @Override
            protected FilterResults performFiltering(CharSequence constraint) {
                FilterResults results = new FilterResults();
                List<String> filteredResults = new ArrayList<>();

                if (constraint == null || constraint.length() == 0) {
                    // إذا لم يكن هناك نص إدخال، عرض القائمة الأصلية
                    filteredResults.addAll(gameList);
                } else {
                    // تنفيذ البحث: تصفية القائمة بناءً على النص المدخل
                    String filterPattern = constraint.toString().toLowerCase().trim();
                    for (String game : gameList) {
                        if (game.toLowerCase().contains(filterPattern)) {
                            filteredResults.add(game);  // إضافة العناصر التي تحتوي على النص المدخل
                        }
                    }
                }

                results.values = filteredResults;
                results.count = filteredResults.size();
                return results;
            }

            @Override
            protected void publishResults(CharSequence constraint, FilterResults results) {
                // تحديث القائمة المفلترة وإعلام الـ Adapter
                filteredGameList.clear();
                filteredGameList.addAll((List) results.values);
                notifyDataSetChanged();  // تحديث العرض
            }
        };
    }
}