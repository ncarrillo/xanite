package com.example.xemu;

import android.app.Application;
import android.net.Uri;
import android.util.Log;
import androidx.documentfile.provider.DocumentFile;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import java.util.HashMap;
import java.util.Locale;

public class GameViewModel extends AndroidViewModel {

    private static final String TAG = "GameViewModel";
    private final MutableLiveData<HashMap<String, String>> gamesLiveData = new MutableLiveData<>();
    private final MutableLiveData<String> errorLiveData = new MutableLiveData<>();

    public GameViewModel(@NonNull Application application) {
        super(application);
    }

    public LiveData<HashMap<String, String>> getGamesLiveData() {
        return gamesLiveData;
    }

    public LiveData<String> getErrorLiveData() {
        return errorLiveData;
    }

    public void loadGames(String folderPath) {
        new Thread(() -> {
            HashMap<String, String> games = new HashMap<>();
            DocumentFile folder = DocumentFile.fromTreeUri(getApplication(), Uri.parse(folderPath));

            if (folder == null || !folder.isDirectory()) {
                errorLiveData.postValue("Invalid folder path");
                return;
            }

            for (DocumentFile file : folder.listFiles()) {
                try {
                    if (file.isFile() && isValidGameFile(file)) {
                        String name = file.getName();
                        games.put(name, file.getUri().toString());
                    }
                } catch (SecurityException e) {
                    Log.w(TAG, "Access denied to: " + file.getUri());
                }
            }

            if (games.isEmpty()) {
                errorLiveData.postValue(getApplication().getString(R.string.no_games_found));
            } else {
                gamesLiveData.postValue(games);
            }
        }).start();
    }

    private boolean isValidGameFile(DocumentFile file) {
        String fileName = file.getName();
        if (fileName == null) return false;
        String lowerName = fileName.toLowerCase(Locale.ROOT);
        String mimeType = file.getType();
        return lowerName.endsWith(".iso") ||
                "application/x-iso9660-image".equals(mimeType);
    }
}