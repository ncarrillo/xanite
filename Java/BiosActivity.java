package com.example.xemu;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

public class BiosActivity extends AppCompatActivity {

    static {
        System.loadLibrary("bios_lib"); 
    }

    public native boolean loadBios(String biosFilePath);

    private static final int PICK_BIOS_FILE_REQUEST = 1;
    private static final int PICK_BOOT_ROM_FILE_REQUEST = 2;
    private static final int PICK_HDD_IMAGE_FILE_REQUEST = 3;
    private static final int PERMISSION_REQUEST_CODE = 100;

    private static final String BIOS_FILE_NAME = "Complex_4627v1.03.bin";
    private static final String BOOT_ROM_FILE_NAME = "mcpx_1.0.bin";
    private static final String HDD_IMAGE_FILE_NAME = "xbox_hdd.qcow2";

    private boolean isBiosLoaded = false;
    private boolean isBootRomLoaded = false;
    private boolean isHddImageLoaded = false;

    private String biosFilePath;
    private String bootRomFilePath;
    private String hddImageFilePath;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_bios);

        hideSystemUI();
        checkStoragePermissions();

        Button addBiosButton = findViewById(R.id.add_bios_button);
        addBiosButton.setOnClickListener(v -> openFileChooser(PICK_BIOS_FILE_REQUEST));

        Button addBootRomButton = findViewById(R.id.add_boot_rom_button);
        addBootRomButton.setOnClickListener(v -> openFileChooser(PICK_BOOT_ROM_FILE_REQUEST));

        Button addHddImageButton = findViewById(R.id.add_hard_disk_button);
        addHddImageButton.setOnClickListener(v -> openFileChooser(PICK_HDD_IMAGE_FILE_REQUEST));

        Button nextButton = findViewById(R.id.next_button);
        nextButton.setOnClickListener(v -> {
            if (isBiosLoaded && isBootRomLoaded && isHddImageLoaded) {
                // تغيير النشاط المستهدف إلى GameSelectionActivity
                Intent intent = new Intent(BiosActivity.this, GameSelectionActivity.class);
                intent.putExtra("BIOS_PATH", biosFilePath);
                intent.putExtra("BOOT_ROM_PATH", bootRomFilePath);
                intent.putExtra("HDD_IMAGE_PATH", hddImageFilePath);
                startActivity(intent);
            } else {
                Toast.makeText(BiosActivity.this, "Please put the BIOS, Boot ROM, and the Xbox HDD Image.", Toast.LENGTH_LONG).show();
            }
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

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode == RESULT_OK && data != null) {
            Uri fileUri = data.getData();
            if (fileUri != null) {
                String fileName = FileUtils.getFileName(this, fileUri);
                String filePath = FileUtils.getPathFromUri(this, fileUri);

                switch (requestCode) {
                    case PICK_BIOS_FILE_REQUEST:
                        if (BIOS_FILE_NAME.equals(fileName)) {
                            boolean success = loadBios(filePath);
                            if (success) {
                                isBiosLoaded = true;
                                biosFilePath = filePath;
                                Toast.makeText(this, "✅", Toast.LENGTH_SHORT).show();
                            } else {
                                Toast.makeText(this, "Failed BIOS", Toast.LENGTH_SHORT).show();
                            }
                        } else {
                            Toast.makeText(this, "Invalid BIOS file! Please select the correct BIOS file.", Toast.LENGTH_LONG).show();
                        }
                        break;

                    case PICK_BOOT_ROM_FILE_REQUEST:
                        if (BOOT_ROM_FILE_NAME.equals(fileName)) {
                            isBootRomLoaded = true;
                            bootRomFilePath = filePath;
                            Toast.makeText(this, "✅", Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, "Failed Boot ROM", Toast.LENGTH_LONG).show();
                        }
                        break;

                    case PICK_HDD_IMAGE_FILE_REQUEST:
                        if (HDD_IMAGE_FILE_NAME.equals(fileName)) {
                            isHddImageLoaded = true;
                            hddImageFilePath = filePath;
                            Toast.makeText(this, "✅", Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, "Failed Xbox HDD Image", Toast.LENGTH_LONG).show();
                        }
                        break;

                    default:
                        Toast.makeText(this, "Invalid file selected!", Toast.LENGTH_LONG).show();
                        break;
                }
            }
        }
    }

    private void openFileChooser(int requestCode) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("*/*");
        startActivityForResult(intent, requestCode);
    }

    private void checkStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED ||
                checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, PERMISSION_REQUEST_CODE);
            }
        }
    }
}