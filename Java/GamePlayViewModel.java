package com.example.xemu;

import android.app.Application;
import android.content.Context;
import android.content.Intent;  // Intent
import android.net.Uri;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import java.io.BufferedInputStream;
import java.io.InputStream;
import java.security.MessageDigest;

public class GamePlayViewModel extends AndroidViewModel {

    private static final String TAG = "GamePlayViewModel";
    private final MutableLiveData<Integer> progressLiveData = new MutableLiveData<>();
    private final MutableLiveData<String> errorLiveData = new MutableLiveData<>();
    private final MutableLiveData<Uri> successLiveData = new MutableLiveData<>();

    static {
        System.loadLibrary("nativ_lib");
    }

    public GamePlayViewModel(@NonNull Application application) {
        super(application);
    }

    public LiveData<Integer> getProgressLiveData() {
        return progressLiveData;
    }

    public LiveData<String> getErrorLiveData() {
        return errorLiveData;
    }

    public LiveData<Uri> getSuccessLiveData() {
        return successLiveData;
    }

    public void processGameUri(Context context, Uri gameUri) {
        new Thread(() -> {
            try {
                if (!verifyUriAccess(context, gameUri)) {
                    errorLiveData.postValue("File access denied");
                    return;
                }

                if (!verifyChecksum(context, gameUri)) {
                    errorLiveData.postValue("Checksum verification failed");
                    return;
                }

                if (!nativLoadGame(context, gameUri)) {
                    errorLiveData.postValue("Failed to load game");
                    return;
                }

                successLiveData.postValue(gameUri);

                // بدء النشاط لعرض اللعبة
                Intent intent = new Intent(context, GameDisplayActivity.class);
                intent.setData(gameUri);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);

            } catch (Exception e) {
                Log.e(TAG, "Game processing failed", e);
                errorLiveData.postValue("Error: " + e.getMessage());
            }
        }).start();
    }

    private boolean verifyUriAccess(Context context, Uri uri) {
        try (InputStream stream = context.getContentResolver().openInputStream(uri)) {
            return stream != null;
        } catch (Exception e) {
            Log.e(TAG, "URI access verification failed", e);
            return false;
        }
    }

    private boolean verifyChecksum(Context context, Uri uri) {
        try (InputStream inputStream = context.getContentResolver().openInputStream(uri);
             BufferedInputStream bufferedInputStream = new BufferedInputStream(inputStream)) {

            long totalSize = inputStream.available();
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] buffer = new byte[1024 * 1024];
            int bytesRead;
            while ((bytesRead = bufferedInputStream.read(buffer)) != -1) {
                digest.update(buffer, 0, bytesRead);
                updateProgress(totalSize, bufferedInputStream.available());
            }
            String checksum = bytesToHex(digest.digest());
            Log.d(TAG, "Checksum: " + checksum);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Checksum verification failed", e);
            return false;
        }
    }

    private void updateProgress(long totalSize, long remainingBytes) {
        int progress = (int) (((totalSize - remainingBytes) / (double) totalSize) * 100);
        progressLiveData.postValue(progress);
    }

    private String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    private native boolean nativLoadGame(Context context, Uri uri);

    public native String getExampleString();
}