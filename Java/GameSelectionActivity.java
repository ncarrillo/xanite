package com.example.xemu;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import android.Manifest;
import android.content.pm.PackageManager;
import androidx.documentfile.provider.DocumentFile;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public class GameSelectionActivity extends AppCompatActivity {

    private static final int PICK_GAME_FOLDER_REQUEST = 1;
    private static final int PERMISSION_REQUEST_CODE = 100;
    private String gameFolderPath = null;
    private TextView gamePathTextView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_game_selection);

        // التحقق من الأذونات
        checkStoragePermissions();

        gamePathTextView = findViewById(R.id.game_path_text_view);

        // فتح مجلد اختيار الألعاب عند الضغط على الزر
        Button addGameButton = findViewById(R.id.add_game_button);
        addGameButton.setOnClickListener(v -> openFolderSelector());

        // التعامل مع الضغط على زر "التالي"
        Button nextButton = findViewById(R.id.next_button);
        nextButton.setOnClickListener(v -> {
            if (gameFolderPath != null) {
                // تشفير مسار المجلد قبل إرساله
                String encryptedFolderPath = encryptFilePath(gameFolderPath);

                // استخراج أسماء ملفات الألعاب من المجلد
                ArrayList<String> gameFiles = getGameFilesFromFolder(gameFolderPath);
                
                if (!gameFiles.isEmpty()) {
                    // إرسال أسماء الألعاب إلى ShowGameActivity
                    Intent showGameIntent = new Intent(GameSelectionActivity.this, ShowGameActivity.class);
                    showGameIntent.putExtra("encryptedFolderPath", encryptedFolderPath);  // إرسال المسار المشفر
                    showGameIntent.putStringArrayListExtra("gameFiles", gameFiles);  // إرسال قائمة أسماء الألعاب
                    startActivity(showGameIntent);
                } else {
                    Toast.makeText(GameSelectionActivity.this, "No .iso or .xiso files found", Toast.LENGTH_SHORT).show();
                }
            } else {
                Toast.makeText(GameSelectionActivity.this, "Please select a game folder first", Toast.LENGTH_SHORT).show();
            }
        });
    }

    // فتح واجهة اختيار المجلد
    private void openFolderSelector() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        startActivityForResult(intent, PICK_GAME_FOLDER_REQUEST);
    }

    // التحقق من الأذونات
    private void checkStoragePermissions() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED ||
            ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, PERMISSION_REQUEST_CODE);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "Permissions granted", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "Permissions denied", Toast.LENGTH_SHORT).show();
            }
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode == RESULT_OK && data != null && requestCode == PICK_GAME_FOLDER_REQUEST) {
            Uri folderUri = data.getData();
            if (folderUri != null) {
                DocumentFile documentFolder = DocumentFile.fromTreeUri(this, folderUri);
                if (documentFolder != null && documentFolder.isDirectory()) {
                    gameFolderPath = folderUri.toString();  // حفظ مسار المجلد

                    // عرض اسم المجلد فقط
                    gamePathTextView.setText(documentFolder.getName());
                }
            }
        }
    }

    // تشفير المسار
    private String encryptFilePath(String filePath) {
        byte[] encodedBytes = Base64.encode(filePath.getBytes(StandardCharsets.UTF_8), Base64.DEFAULT);
        return new String(encodedBytes, StandardCharsets.UTF_8);
    }

    // استخراج أسماء ملفات الألعاب من المجلد
    private ArrayList<String> getGameFilesFromFolder(String folderUri) {
        ArrayList<String> gameFiles = new ArrayList<>();
        Uri uri = Uri.parse(folderUri);
        DocumentFile documentFolder = DocumentFile.fromTreeUri(this, uri);

        if (documentFolder != null && documentFolder.isDirectory()) {
            for (DocumentFile file : documentFolder.listFiles()) {
                if (file.isFile() && (file.getName().endsWith(".iso") || file.getName().endsWith(".xiso"))) {
                    gameFiles.add(file.getName());  // إضافة اسم الملف فقط
                }
            }
        }

        return gameFiles;
    }
}