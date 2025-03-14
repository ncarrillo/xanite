package com.example.xemu;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.Choreographer;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import android.content.pm.ConfigurationInfo;

public class GameDisplayActivity extends AppCompatActivity {

    private static final String TAG = "GameDisplayActivity";
    private GLSurfaceView glSurfaceView;
    private GameRenderer gameRenderer;
    private TextView fpsTextView;
    private TextView shaderInfoTextView;
    private VirtualJoystick virtualJoystick;
    private FrameLayout joystickContainer;
    private ProgressBar progressBar;
    private ExecutorService executorService;
    private volatile boolean isRunning = true;

    static {
        try {
            System.loadLibrary("nati_lib");
            System.loadLibrary("cdio");
            System.loadLibrary("iso9660");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Native libraries failed to load", e);
            // Toast.makeText(App.getContext(), "Failed to load native libraries", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_game_display);

        Intent intent = getIntent();
        Uri isoUri = intent.getData();

        if (isoUri == null) {
            showErrorAndFinish("No ISO file provided");
            return;
        }

        Log.d(TAG, "ISO URI: " + isoUri.toString());

        setupFullscreen();
        initViews();
        initOpenGL();
        initFPSDisplay();
        initShaderInfoDisplay();
        initVirtualJoystick();
        loadAndRenderGame(isoUri);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
        }
    }

    private void setupFullscreen() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    private void initViews() {
        glSurfaceView = findViewById(R.id.glSurfaceView);
        fpsTextView = findViewById(R.id.fpsTextView);
        shaderInfoTextView = findViewById(R.id.shaderInfoTextView);
        joystickContainer = findViewById(R.id.joystickContainer);
        progressBar = findViewById(R.id.progressBar);

        if (glSurfaceView == null || fpsTextView == null || shaderInfoTextView == null || joystickContainer == null || progressBar == null) {
            showErrorAndFinish("Failed to initialize views");
        }
    }

    private void initOpenGL() {
        if (glSurfaceView == null) {
            showErrorAndFinish("GLSurfaceView not found in layout");
            return;
        }

        ActivityManager activityManager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo configurationInfo = activityManager.getDeviceConfigurationInfo();
        if (configurationInfo.reqGlEsVersion < 0x30000) {
            showErrorAndFinish("OpenGL ES 3.0 is not supported on this device");
            return;
        }

        glSurfaceView.setEGLContextClientVersion(3);

        gameRenderer = new GameRenderer(this);
        if (gameRenderer == null) {
            showErrorAndFinish("Failed to initialize GameRenderer");
            return;
        }

        glSurfaceView.setRenderer(gameRenderer);
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }

    private void initFPSDisplay() {
        if (fpsTextView == null) {
            Log.w(TAG, "FPS TextView not found in layout");
            return;
        }

        Choreographer.getInstance().postFrameCallback(new Choreographer.FrameCallback() {
            long lastFrameTime = 0;

            @Override
            public void doFrame(long frameTimeNanos) {
                if (lastFrameTime != 0) {
                    long frameDuration = frameTimeNanos - lastFrameTime;
                    double fps = 1_000_000_000.0 / frameDuration;
                    runOnUiThread(() -> fpsTextView.setText("FPS: " + fps));
                }
                lastFrameTime = frameTimeNanos;
                Choreographer.getInstance().postFrameCallback(this);
            }
        });
    }

    private void initShaderInfoDisplay() {
        if (shaderInfoTextView == null) {
            Log.w(TAG, "Shader Info TextView not found in layout");
            return;
        }

        new Thread(() -> {
            while (isRunning) {
                try {
                    Thread.sleep(1000);
                    runOnUiThread(() -> {
                        if (gameRenderer != null) {
                            shaderInfoTextView.setText("Shader: " + gameRenderer.getShaderInfo());
                        }
                    });
                } catch (InterruptedException e) {
                    Log.e(TAG, "Shader info update thread interrupted", e);
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        }).start();
    }

    private void initVirtualJoystick() {
        if (joystickContainer == null) {
            Log.w(TAG, "Joystick container not found in layout");
            return;
        }
        virtualJoystick = new VirtualJoystick(this);
        joystickContainer.addView(virtualJoystick);
        virtualJoystick.setJoystickListener((x, y) -> {
            if (gameRenderer != null) {
                gameRenderer.handleJoystickInput(x, y);
            }
        });
    }

    private void loadAndRenderGame(Uri isoUri) {
        if (progressBar != null) {
            progressBar.setVisibility(View.VISIBLE);
        }

        executorService = Executors.newSingleThreadExecutor();
        executorService.execute(() -> {
            runOnUiThread(() -> Toast.makeText(this, "Loading game...", Toast.LENGTH_SHORT).show());

            boolean loaded = gameRenderer.loadISO(isoUri.toString());
            if (!loaded) {
                showError("Failed to load game files");
                runOnUiThread(() -> {
                    if (progressBar != null) {
                        progressBar.setVisibility(View.GONE);
                    }
                });
                return;
            }

            runOnUiThread(() -> {
                Toast.makeText(this, "Game loaded successfully", Toast.LENGTH_SHORT).show();
                if (progressBar != null) {
                    progressBar.setVisibility(View.GONE);
                }

                if (glSurfaceView != null) {
                    glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
                    glSurfaceView.requestRender();

                    if (gameRenderer != null) {
                        gameRenderer.onGameLoaded();
                    }

                    glSurfaceView.setPreserveEGLContextOnPause(true);
                }
            });
        });
    }

    private void showError(String message) {
        runOnUiThread(() -> {
            Log.e(TAG, message);
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
        });
    }

    private void showErrorAndFinish(String message) {
        showError(message);
        finish();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        isRunning = false;
        if (executorService != null) {
            executorService.shutdown();
        }
        if (joystickContainer != null && virtualJoystick != null) {
            joystickContainer.removeView(virtualJoystick);
        }
        if (gameRenderer != null) {
            gameRenderer.cleanup();
        }
    }
}