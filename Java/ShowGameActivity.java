package com.example.xemu;

import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.SearchView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;

public class ShowGameActivity extends AppCompatActivity {

    private static final String TAG = "ShowGameActivity";
    private static final String EXTRA_ENCRYPTED_FOLDER_PATH = "encryptedFolderPath";
    private static final String EXTRA_GAME_PATH = "GAME_PATH";

    private TextView gameNameTextView;
    private ListView gameListView;
    private SearchView gameSearchView;
    private View infoButton;
    private GameAdapter gameAdapter;
    private ArrayList<String> gameNamesList;
    private HashMap<String, String> gamePathsMap;
    private String gameFilePath;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_show_game);
        hideSystemUI();
        initializeViews();
        setupInfoButton();

        // تهيئة القائمة والمحول
        gameNamesList = new ArrayList<>();
        gamePathsMap = new HashMap<>();
        gameAdapter = new GameAdapter(this, gameNamesList);
        gameListView.setAdapter(gameAdapter);

        // استلام مسار المجلد المشفر وفك تشفيره
        String encryptedFilePath = getIntent().getStringExtra(EXTRA_ENCRYPTED_FOLDER_PATH);
        if (encryptedFilePath != null && !encryptedFilePath.isEmpty()) {
            gameFilePath = decryptFilePath(encryptedFilePath);
            Log.d(TAG, "Decrypted file path: " + gameFilePath);
        }

        // جلب ملفات الألعاب ذات الامتداد .iso من المجلد المحدد
        if (gameFilePath != null && !gameFilePath.isEmpty()) {
            HashMap<String, String> gameFiles = getGameFilesFromFolder(gameFilePath);
            if (!gameFiles.isEmpty()) {
                gameNamesList.addAll(gameFiles.keySet());
                gamePathsMap.putAll(gameFiles);
                gameAdapter.notifyDataSetChanged();
            } else {
                Toast.makeText(this, "No .iso files found", Toast.LENGTH_SHORT).show();
            }
        } else {
            Toast.makeText(this, "Game folder path is missing", Toast.LENGTH_SHORT).show();
        }

        // إعداد مستمع النقر على عناصر القائمة
        gameListView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                String selectedName = gameNamesList.get(position);
                String fullUriString = gamePathsMap.get(selectedName);
                if (fullUriString != null && !fullUriString.isEmpty()) {
                    Uri isoFileUri = Uri.parse(fullUriString);
                    // استخدام AsyncTask لنسخ الملف في الخلفية دون تعيين اسم افتراضي
                    new CopyFileTask().execute(isoFileUri);
                } else {
                    Toast.makeText(ShowGameActivity.this, "ISO file path is missing.", Toast.LENGTH_SHORT).show();
                }
            }
        });

        // إعداد البحث وتصفية القائمة أثناء الكتابة
        gameSearchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                // لا نقوم بعمل شيء عند تقديم النص
                return false;
            }

            @Override
            public boolean onQueryTextChange(String newText) {
                gameAdapter.getFilter().filter(newText);
                return false;
            }
        });
    }

    /**
     * تهيئة المكونات من ملف XML.
     */
    private void initializeViews() {
        gameNameTextView = findViewById(R.id.game_name_text_view);
        gameListView = findViewById(R.id.game_list_view);
        gameSearchView = findViewById(R.id.game_search_view);
        infoButton = findViewById(R.id.info_button);
    }

    /**
     * إعداد زر المعلومات لفتح شاشة المعلومات.
     */
    private void setupInfoButton() {
        infoButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(ShowGameActivity.this, InfoActivity.class);
                startActivity(intent);
            }
        });
    }

    /**
     * إخفاء عناصر النظام لعرض واجهة ملء الشاشة.
     */
    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    /**
     * فك تشفير مسار المجلد المشفر باستخدام Base64.
     *
     * @param encryptedFilePath المسار المشفر.
     * @return المسار المفكك تشفيره.
     */
    private String decryptFilePath(String encryptedFilePath) {
        byte[] decodedBytes = Base64.decode(encryptedFilePath, Base64.DEFAULT);
        return new String(decodedBytes, StandardCharsets.UTF_8);
    }

    /**
     * جلب ملفات الألعاب ذات الامتداد .iso من المجلد المحدد.
     *
     * @param folderUri مسار المجلد على شكل URI.
     * @return خريطة تحتوي على أسماء الملفات ومساراتها (بـ URI).
     */
    private HashMap<String, String> getGameFilesFromFolder(String folderUri) {
        HashMap<String, String> gameFiles = new HashMap<>();
        DocumentFile folder = DocumentFile.fromTreeUri(this, Uri.parse(folderUri));

        if (folder != null && folder.isDirectory()) {
            for (DocumentFile file : folder.listFiles()) {
                if (file.isFile() && file.getName() != null && file.getName().endsWith(".iso")) {
                    String fileName = file.getName();
                    String fileUri = file.getUri().toString();
                    gameFiles.put(fileName, fileUri);
                }
            }
        } else {
            Toast.makeText(this, "Invalid folder path or not a directory", Toast.LENGTH_SHORT).show();
        }

        Log.d(TAG, "Game files found: " + gameFiles);
        return gameFiles;
    }

    /**
     * مهمة خلفية (AsyncTask) لنسخ ملف ISO من الـ URI المصدر إلى التخزين الداخلي.
     * يتم هنا استخدام AsyncTask لضمان سرعة الاستجابة وعدم تجميد واجهة المستخدم.
     */
    private class CopyFileTask extends AsyncTask<Uri, Void, String> {

        @Override
        protected void onPreExecute() {
            LogHelper.writeLog("Started copying file...");
        }

        @Override
        protected String doInBackground(Uri... uris) {
            if (uris.length == 0) return null;
            Uri fileUri = uris[0];
            return copyFileToInternalStorage(fileUri);
        }

        @Override
        protected void onPostExecute(String localFilePath) {
            if (localFilePath != null) {
                LogHelper.writeLog("File transferred successfully: " + localFilePath);
                // بدء نشاط GamePlayActivity مع تمرير مسار الملف
                Intent intent = new Intent(ShowGameActivity.this, GamePlayActivity.class);
                intent.putExtra(EXTRA_GAME_PATH, localFilePath);
                startActivity(intent);
            } else {
                Toast.makeText(ShowGameActivity.this, "Error transferring the game file.", Toast.LENGTH_SHORT).show();
                Log.e(TAG, "File transfer failed.");
                LogHelper.writeLog("Error transferring file.");
            }
        }
    }

    /**
     * تنقل الملف من الـ URI المصدر إلى التخزين الداخلي للتطبيق باستخدام اسم الملف الأصلي.
     * إذا لم يتمكن التطبيق من الحصول على اسم الملف الأصلي، يتم اعتبار العملية فاشلة.
     *
     * @param fileUri URI المصدر للملف.
     * @return المسار الكامل للملف المنقول في التخزين الداخلي أو null في حال حدوث خطأ.
     */
    private String copyFileToInternalStorage(Uri fileUri) {
        InputStream inputStream = null;
        FileOutputStream outputStream = null;
        try {
            inputStream = getContentResolver().openInputStream(fileUri);
            if (inputStream == null) {
                Log.e(TAG, "Unable to open InputStream from URI: " + fileUri);
                return null;
            }
            // الحصول على اسم الملف الأصلي من URI
            DocumentFile documentFile = DocumentFile.fromSingleUri(this, fileUri);
            if (documentFile == null || documentFile.getName() == null) {
                Log.e(TAG, "Unable to retrieve the file name from URI: " + fileUri);
                return null;
            }
            String fileName = documentFile.getName();
            File destinationFile = new File(getFilesDir(), fileName);

            byte[] buffer = new byte[8192];
             int bytesRead;
           outputStream = new FileOutputStream(destinationFile);
while ((bytesRead = inputStream.read(buffer)) != -1) {
    outputStream.write(buffer, 0, bytesRead);
}
outputStream.flush();
Log.d(TAG, "File copied to internal storage: " + destinationFile.getAbsolutePath());
return destinationFile.getAbsolutePath();
} catch (IOException e) {
    Log.e(TAG, "Error copying file: ", e);
    return null;
} finally {
    try {
        if (inputStream != null)
            inputStream.close();
    } catch (IOException ignored) {
    }
    try {
        if (outputStream != null)
            outputStream.close();
    } catch (IOException ignored) {
    }
}
}
}