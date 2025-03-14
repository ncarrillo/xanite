package com.example.xemu;
 //pls this not fake I............tired..... :(
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        hideSystemUI();

        SharedPreferences sharedPreferences = getSharedPreferences("AppState", MODE_PRIVATE);
        boolean isInShowGameActivity = sharedPreferences.getBoolean("isInShowGameActivity", false);

        if (isInShowGameActivity) {
            Intent intent = new Intent(this, ShowGameActivity.class);
            startActivity(intent);
            finish(); // end MainActivity :O
        }

        Button nextButton = findViewById(R.id.next_button);
        nextButton.setOnClickListener(v -> {
            initializeEmulator();
            loadPackages();

            String filePath = getExternalFilesDir(null) + "/xanite/files/";
            String message = "File created in: " + filePath;
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();

            Intent intent = new Intent(MainActivity.this, BiosActivity.class);
            startActivity(intent);
        });
    }

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

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    private void initializeEmulator() {
        String[] paths = {
                "xanite",
                "xanite/Disc_Hdd1",
                "xanite/Disc_Hdd2",
                "xanite/Disc_Hdd3",
                "xanite/biso_sdd",
                "xanite/x-iso9660-image",
        };

        for (String path : paths) {
            File directory = new File(getExternalFilesDir(null), path);
            if (!directory.exists()) {
                directory.mkdirs();
            }
        }
    
        File iconsDir = new File(getExternalFilesDir(null), "xanite/files/Disc_Hdd3");
        extractAssetsDir("assets", iconsDir);

        File nomediaFile = new File(getExternalFilesDir(null), "xanite./Disc");
        try {
            if (!nomediaFile.exists()) {
                nomediaFile.createNewFile();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

      private void loadPackages() {
    }

    private void extractAssetsDir(String assetDir, File outputDir) {
    }
}