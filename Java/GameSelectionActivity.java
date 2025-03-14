package com.example.xemu;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.documentfile.provider.DocumentFile;

import android.Manifest;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class GameSelectionActivity extends AppCompatActivity {

    private static final int PICK_GAME_FOLDER_REQUEST = 1;
    private static final int PERMISSION_REQUEST_CODE = 100;
    private static final String PREFS_NAME = "xanitePrefs";
    private static final String KEY_SAVED_GAME_FOLDER_PATH = "savedGameFolderPath";

    private String gameFolderPath = null;
    private TextView gamePathTextView;
    private SharedPreferences sharedPreferences;
    private ExecutorService executorService;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_game_selection);

        initializeViews();
        checkStoragePermissions(); 
        loadSavedGameFolderPath();

        setupButtonListeners();

        executorService = Executors.newSingleThreadExecutor();
    }

    private void initializeViews() {
        gamePathTextView = findViewById(R.id.game_path_text_view);
    }

    private void loadSavedGameFolderPath() {
        sharedPreferences = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        gameFolderPath = sharedPreferences.getString(KEY_SAVED_GAME_FOLDER_PATH, null);

        if (gameFolderPath != null) {
            updateGamePathTextView(gameFolderPath);
        }
    }

    private void updateGamePathTextView(String folderPath) {
        try {
            Uri savedUri = Uri.parse(folderPath);
            DocumentFile documentFolder = DocumentFile.fromTreeUri(this, savedUri);
            if (documentFolder != null && documentFolder.exists()) {
                gamePathTextView.setText(documentFolder.getName());
            } else {
                gamePathTextView.setText("Invalid folder path");
            }
        } catch (Exception e) {
            Toast.makeText(this, "Error loading folder path", Toast.LENGTH_SHORT).show();
        }
    }

    private void setupButtonListeners() {
        Button addGameButton = findViewById(R.id.add_game_button);
        addGameButton.setOnClickListener(v -> openFolderSelector());

        Button nextButton = findViewById(R.id.next_button);
        nextButton.setOnClickListener(v -> handleNextButtonClick());
    }

    private void openFolderSelector() {
        if (hasStoragePermissions()) {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(intent, PICK_GAME_FOLDER_REQUEST);
        } else {
            Toast.makeText(this, "Storage permission is required to select a folder", Toast.LENGTH_SHORT).show();
        }
    }

    private void handleNextButtonClick() {
        if (gameFolderPath == null) {
            Toast.makeText(this, "Please select a game folder first", Toast.LENGTH_SHORT).show();
            return;
        }

        executorService.execute(() -> {
            List<String> gameFiles = getGameFilesAndPathsFromFolder(gameFolderPath, false);
            List<String> gameFilesPaths = getGameFilesAndPathsFromFolder(gameFolderPath, true);

            runOnUiThread(() -> {
                if (gameFiles.isEmpty()) {
                    Toast.makeText(this, "No ISO files found", Toast.LENGTH_SHORT).show();
                } else {
                    String encryptedFolderPath = encryptFilePath(gameFolderPath);
                    navigateToShowGameActivity(encryptedFolderPath, gameFiles, gameFilesPaths);
                }
            });
        });
    }

    private void navigateToShowGameActivity(String encryptedFolderPath, List<String> gameFiles, List<String> gameFilesPaths) {
        Intent showGameIntent = new Intent(this, ShowGameActivity.class);
        showGameIntent.putExtra("encryptedFolderPath", encryptedFolderPath);
        showGameIntent.putStringArrayListExtra("gameFiles", new ArrayList<>(gameFiles));
        showGameIntent.putStringArrayListExtra("gameFilesPaths", new ArrayList<>(gameFilesPaths));
        startActivity(showGameIntent);
    }

    private boolean hasStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                   return true; 
        } else {
            return ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED &&
                   ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        }
    }

    private void checkStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (!hasStoragePermissions()) {
                ActivityCompat.requestPermissions(this, 
                    new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 
                    PERMISSION_REQUEST_CODE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode == RESULT_OK && data != null && requestCode == PICK_GAME_FOLDER_REQUEST) {
            Uri folderUri = data.getData();
            if (folderUri != null) {
                handleSelectedFolder(folderUri);
            }
        }
    }

    private void handleSelectedFolder(Uri folderUri) {
        try {
            getContentResolver().takePersistableUriPermission(folderUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            DocumentFile documentFolder = DocumentFile.fromTreeUri(this, folderUri);
            if (documentFolder != null && documentFolder.isDirectory() && documentFolder.exists()) {
                gameFolderPath = folderUri.toString();
                updateGamePathTextView(gameFolderPath);
                saveGameFolderPath(gameFolderPath);
            } else {
                Toast.makeText(this, "Invalid folder selected", Toast.LENGTH_SHORT).show();
            }
        } catch (Exception e) {
            Toast.makeText(this, "Error accessing folder", Toast.LENGTH_SHORT).show();
        }
    }

    private void saveGameFolderPath(String folderPath) {
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putString(KEY_SAVED_GAME_FOLDER_PATH, folderPath);
        editor.apply();
    }

    private String encryptFilePath(String filePath) {
        try {
            byte[] encodedBytes = Base64.encode(filePath.getBytes(StandardCharsets.UTF_8), Base64.NO_WRAP);
            return new String(encodedBytes, StandardCharsets.UTF_8);
        } catch (Exception e) {
            Toast.makeText(this, "Error encrypting file path", Toast.LENGTH_SHORT).show();
            return filePath; // fails you:3
        }
    }

    private List<String> getGameFilesAndPathsFromFolder(String folderUri, boolean returnPaths) {
        List<String> gameFiles = new ArrayList<>();
        try {
            Uri uri = Uri.parse(folderUri);
            DocumentFile documentFolder = DocumentFile.fromTreeUri(this, uri);
            if (documentFolder != null && documentFolder.isDirectory() && documentFolder.exists()) {
                for (DocumentFile file : documentFolder.listFiles()) {
                    if (file.isFile() && isIsoFile(file)) {
                        if (returnPaths) {
                            gameFiles.add(file.getUri().toString());
                        } else {
                            gameFiles.add(file.getName());
                        }
                    }
                }
            }
        } catch (Exception e) {
            Toast.makeText(this, "Error reading folder contents", Toast.LENGTH_SHORT).show();
        }
        return gameFiles;
    }

    private boolean isIsoFile(DocumentFile file) {
        try {
            String fileName = file.getName();
            String lowerName = fileName != null ? fileName.toLowerCase() : "";
            String mimeType = getContentResolver().getType(file.getUri());
            return lowerName.endsWith(".iso") || "application/x-iso9660-image".equals(mimeType);
        } catch (Exception e) {
            return false;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (executorService != null) {
            executorService.shutdown();
        }
    }
}