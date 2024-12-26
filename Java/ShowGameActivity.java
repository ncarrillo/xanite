package com.example.xemu;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Base64;
import android.view.View;
import android.net.Uri;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.SearchView;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.documentfile.provider.DocumentFile;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public class ShowGameActivity extends AppCompatActivity {

    private TextView gameNameTextView;
    private ListView gameListView;
    private GameAdapter gameAdapter;
    private ArrayList<String> gameList;
    private String gameFilePath;
    private SearchView gameSearchView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_show_game);

        // إخفاء واجهة النظام
        hideSystemUI();

        gameNameTextView = findViewById(R.id.game_name_text_view);
        gameListView = findViewById(R.id.game_list_view);
        gameSearchView = findViewById(R.id.game_search_view);  // SearchView

        // تهيئة القائمة والـ Adapter
        gameList = new ArrayList<>();
        gameAdapter = new GameAdapter(this, gameList);
        gameListView.setAdapter(gameAdapter);

        // استلام المسار المشفر من GameSelectionActivity
        String encryptedFilePath = getIntent().getStringExtra("encryptedFolderPath");

        // فك تشفير المسار
        if (encryptedFilePath != null && !encryptedFilePath.isEmpty()) {
            gameFilePath = decryptFilePath(encryptedFilePath);
        }

        if (gameFilePath != null && !gameFilePath.isEmpty()) {
            // إضافة ملفات الألعاب بصيغ .iso و .xiso إلى القائمة
            ArrayList<String> gameFiles = getGameFilesFromFolder(gameFilePath);
            if (!gameFiles.isEmpty()) {
                gameList.addAll(gameFiles);
                gameAdapter.notifyDataSetChanged();
            } else {
                Toast.makeText(this, "No .iso or .xiso files found", Toast.LENGTH_SHORT).show();
            }
        } else {
            Toast.makeText(this, "Game folder path is missing", Toast.LENGTH_SHORT).show();
        }

        // إضافة مستمع للنقر على عناصر القائمة
        gameListView.setOnItemClickListener((parent, view, position, id) -> startGamePlay(gameList.get(position)));

        // إعداد الـ SearchView للبحث في قائمة الألعاب
        gameSearchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                // يمكن تنفيذ أي إجراء عند إرسال النص
                return false;
            }

            @Override
            public boolean onQueryTextChange(String newText) {
                // تنفيذ البحث عندما يتغير النص
                gameAdapter.getFilter().filter(newText);
                return false;
            }
        });
    }

    // دالة لتشغيل اللعبة
    private void startGamePlay(String isoFilePath) {
        // التحقق من أن مسار ملف ISO صحيح
        if (isoFilePath != null && !isoFilePath.isEmpty()) {
            // بدء GamePlayActivity مع إرسال مسار اللعبة
            Intent intent = new Intent(ShowGameActivity.this, GamePlayActivity.class);
            intent.putExtra("GAME_PATH", isoFilePath);
            startActivity(intent);
        } else {
            Toast.makeText(this, "ISO file path is missing.", Toast.LENGTH_SHORT).show();
        }
    }

    // دالة لإخفاء واجهة النظام
    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    // دالة لفك تشفير المسار
    private String decryptFilePath(String encryptedFilePath) {
        byte[] decodedBytes = Base64.decode(encryptedFilePath, Base64.DEFAULT);
        return new String(decodedBytes, StandardCharsets.UTF_8);
    }

    // دالة لاستخراج ملفات الألعاب بصيغ .iso و .xiso من المجلد
    private ArrayList<String> getGameFilesFromFolder(String folderUri) {
        ArrayList<String> gameFiles = new ArrayList<>();
        DocumentFile folder = DocumentFile.fromTreeUri(this, Uri.parse(folderUri));

        // التأكد من أن المجلد قابل للوصول
        if (folder != null && folder.isDirectory()) {
            for (DocumentFile file : folder.listFiles()) {
                if (file.isFile() && (file.getName().endsWith(".iso") || file.getName().endsWith(".xiso"))) {
                    // إضافة اسم اللعبة (اسم الملف)
                    gameFiles.add(file.getName());
                }
            }
        } else {
            Toast.makeText(this, "Invalid folder path or not a directory", Toast.LENGTH_SHORT).show();
        }

        return gameFiles;
    }
}