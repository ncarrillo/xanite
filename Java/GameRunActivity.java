package com.example.xemu;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Build;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.WindowManager;
import android.opengl.GLSurfaceView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class GameRunActivity extends AppCompatActivity {

    private static final String TAG = "GameRunActivity";
    private GLSurfaceView gameSurfaceView;
    private Emulator emulator;
    private String isoFilePath;
    private String biosFilePath;
    private String mcpxFilePath;
    private String hddImageFilePath;

    private static final String BIOS_FILE = "Complex_4627v1.03.bin";
    private static final String MCPX_FILE = "mcpx_1.0.bin";
    private static final String HDD_IMAGE_FILE = "xbox_hdd.qcow2";

    private boolean isEmulatorRunning = false;

    static {
        try {
            System.loadLibrary("mine_lib");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Error loading native library", e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Enable full screen mode
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

        setContentView(R.layout.activity_gamerun);

        // Initialize UI components
        gameSurfaceView = findViewById(R.id.game_surface_view);

        if (gameSurfaceView == null) {
            showToastAndFinish("Required components are missing!");
            Log.e(TAG, "GLSurfaceView not found. Check your XML layout.");
            return;
        }

        // Get ISO path from the previous activity
        isoFilePath = getIntent().getStringExtra("ISO_PATH");
        String gameTitle = getIntent().getStringExtra("GAME_TITLE");
        String playerName = getIntent().getStringExtra("PLAYER_NAME");

        if (isoFilePath == null || isoFilePath.isEmpty()) {
            showToastAndFinish("Game file missing!");
            return;
        }

        Log.d(TAG, "Game Title: " + gameTitle);
        Log.d(TAG, "Player Name: " + playerName);

        try {
            loadEssentialFiles();

            biosFilePath = getFilesDir().getAbsolutePath() + "/" + BIOS_FILE;
            mcpxFilePath = getFilesDir().getAbsolutePath() + "/" + MCPX_FILE;
            hddImageFilePath = getFilesDir().getAbsolutePath() + "/" + HDD_IMAGE_FILE;

            // Enable OpenGL ES 3.0
            gameSurfaceView.setEGLContextClientVersion(3);

            // Setup renderer with additional settings
            GameRenderer gameRenderer = new GameRenderer(
                    this, // Context
                    gameSurfaceView.getHolder().getSurface(),
                    isoFilePath,
                    biosFilePath,
                    mcpxFilePath,
                    hddImageFilePath
            );

            // Detect GPU type and set appropriate rendering options
            String gpu = getDeviceGpu();
            Log.d(TAG, "Detected GPU: " + gpu);

            gameRenderer.setAccuracy(true);  // Enable accuracy mode
            gameRenderer.setGpuApi("OpenGL_ES_3.0");  // Set OpenGL ES 3.0 as the rendering API
            gameRenderer.setGpu(gpu);  // Set GPU dynamically based on detected GPU
            gameRenderer.setDiskShaderCache(true);  // Enable disk shader caching
            gameRenderer.setAsynchronousShaders(true);  // Enable asynchronous shaders
            gameRenderer.setReactiveFlushing(true);  // Enable reactive flushing

            // Set renderer and render mode
            gameSurfaceView.setRenderer(gameRenderer);
            gameSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

            gameSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    startEmulator();
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                    // Adjust resolution dynamically based on GPU
                    if ("Snapdragon".equalsIgnoreCase(gpu)) {
                        holder.setFixedSize(1280, 720); // Resolution for Snapdragon devices
                    } else if ("Mali".equalsIgnoreCase(gpu)) {
                        holder.setFixedSize(1024, 768); // Resolution for Mali devices
                    } else {
                        holder.setFixedSize(640, 480); // Fallback resolution for unknown GPUs
                    }
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    stopEmulator();
                }
            });

        } catch (Exception e) {
            Log.e(TAG, "Error initializing emulator or loading files", e);
            showToastAndFinish("Failed to initialize emulator");
        }

        // Register broadcast receiver for receiving game status updates
        IntentFilter filter = new IntentFilter("com.example.xemu.UPDATE_GAME_STATUS");
        registerReceiver(gameStatusReceiver, filter);
    }

    private void loadEssentialFiles() throws IOException {
        if (!checkAndCopyFile(BIOS_FILE)) {
            Log.e(TAG, "BIOS file missing");
            throw new IOException("BIOS file missing!");
        }
        if (!checkAndCopyFile(MCPX_FILE)) {
            Log.e(TAG, "MCPX file missing");
            throw new IOException("MCPX file missing!");
        }
        if (!checkAndCopyFile(HDD_IMAGE_FILE)) {
            Log.e(TAG, "HDD file missing");
            throw new IOException("HDD file missing!");
        }
    }

    private boolean checkAndCopyFile(String assetName) throws IOException {
        File outFile = new File(getFilesDir(), assetName);
        if (!outFile.exists()) {
            try (InputStream is = getAssets().open(assetName);
                 FileOutputStream fos = new FileOutputStream(outFile)) {
                byte[] buffer = new byte[1024];
                int read;
                while ((read = is.read(buffer)) != -1) {
                    fos.write(buffer, 0, read);
                }
            } catch (IOException e) {
                Log.e(TAG, "Error copying file: " + assetName, e);
                throw new IOException("Failed to copy asset: " + assetName);
            }
        }
        return outFile.exists();
    }

    private String getDeviceGpu() {
        String gpu = "Unknown";
        try {
            String gpuInfo = Build.HARDWARE;
            if (gpuInfo.contains("qcom") || gpuInfo.contains("snapdragon")) {
                gpu = "Snapdragon";
            } else if (gpuInfo.contains("mali")) {
                gpu = "Mali";
            }
        } catch (Exception e) {
            Log.e(TAG, "Error detecting GPU", e);
        }
        return gpu;
    }

    private void startEmulator() {
        if (emulator == null && !isEmulatorRunning) {
            emulator = new Emulator(this, biosFilePath, mcpxFilePath, hddImageFilePath, isoFilePath);
            emulator.start();
            isEmulatorRunning = true; // Emulator has started
            Log.d(TAG, "Starting emulator...");
        }
    }

    private void stopEmulator() {
        if (emulator != null && isEmulatorRunning) {
            emulator.stop();
            isEmulatorRunning = false; // Emulator stopped
            Log.d(TAG, "Emulator stopped.");
        }
    }

    private void showToastAndFinish(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
        finish();
    }

    @Override
    protected void onPause() {
        super.onPause();
        stopEmulator();  // Explicitly stop the emulator when the activity is paused
    }

    @Override
    protected void onResume() {
        super.onResume();
        // If the emulator is not running, start it again when the activity is resumed
        if (!isEmulatorRunning) {
            startEmulator();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopEmulator();
        // Unregister the broadcast receiver when activity is destroyed
        unregisterReceiver(gameStatusReceiver);
    }

    // BroadcastReceiver for receiving game status updates
    private final BroadcastReceiver gameStatusReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String gameStatus = intent.getStringExtra("GAME_STATUS");
            Log.d(TAG, "Game status updated: " + gameStatus);
        }
    };
}