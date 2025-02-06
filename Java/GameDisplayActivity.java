package com.example.xemu;

import android.content.Intent;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class GameDisplayActivity extends AppCompatActivity {
    private static final String TAG = "GameDisplayActivity";
    private GLSurfaceView glSurfaceView;
    private GameRenderer gameRenderer;

    // Static block for loading native libraries
    static {
        try {
            System.loadLibrary("avcodec");
            System.loadLibrary("avformat");
            System.loadLibrary("swresample");
            System.loadLibrary("swscale");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Native library loading failed: " + e.getMessage());
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_game_display);

        // Initialize OpenGL for rendering the game
        initializeOpenGL();
        
        // Handle ISO file from intent
        handleIntentData();
    }

    // Method to initialize OpenGL and the surface view
    private void initializeOpenGL() {
        try {
            glSurfaceView = findViewById(R.id.glSurfaceView);
            glSurfaceView.setEGLContextClientVersion(3);  // Use OpenGL ES 3.0
            
            // Adjust surface size based on device screen metrics
            DisplayMetrics metrics = new DisplayMetrics();
            getWindowManager().getDefaultDisplay().getMetrics(metrics);

            // Set a max resolution of 1280x720 or fit within screen
            glSurfaceView.getLayoutParams().width = Math.min(1280, metrics.widthPixels);
            glSurfaceView.getLayoutParams().height = Math.min(720, metrics.heightPixels);

            // Initialize the renderer responsible for drawing the game
            gameRenderer = new GameRenderer(this);
            glSurfaceView.setRenderer(gameRenderer);
            glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);  // Render only when needed
            
        } catch (Exception e) {
            handleError("OpenGL initialization failed", e);
        }
    }

    // Method to handle incoming data via intent (ISO file path)
    private void handleIntentData() {
        Intent intent = getIntent();
        if (intent != null) {
            String isoPath = intent.getStringExtra("ISO_PATH");
            if (isoPath != null && !isoPath.isEmpty()) {
                loadGameISO(isoPath);  // Start loading the ISO file
            } else {
                handleError("Invalid ISO path", null);  // Handle the error for invalid path
            }
        }
    }

    // Method to load the game ISO file asynchronously
    private void loadGameISO(String isoPath) {
        new Thread(() -> {
            try {
                // Load the ISO file using the renderer
                gameRenderer.loadISO(isoPath);
                
                // Once ISO is loaded, start the game play activity
                runOnUiThread(() -> startGamePlay(isoPath));
            } catch (Exception e) {
                handleError("ISO loading failed: " + e.getMessage(), e);  // Handle loading errors
            }
        }).start();
    }

    // Method to start the GamePlayActivity once the ISO is loaded
    private void startGamePlay(String isoPath) {
        try {
            Intent gameplayIntent = new Intent(this, GamePlayActivity.class);
            gameplayIntent.putExtra("GAME_PATH", isoPath);  // Pass the ISO path to the gameplay activity
            startActivity(gameplayIntent);  // Start the GamePlayActivity
        } catch (Exception e) {
            handleError("Game launch failed: " + e.getMessage(), e);  // Handle launching errors
        }
    }

    // Method to handle errors, log them, and show a toast message
    private void handleError(String message, Exception e) {
        Log.e(TAG, message + (e != null ? ": " + e.getMessage() : ""));
        runOnUiThread(() -> 
            Toast.makeText(this, message, Toast.LENGTH_LONG).show()
        );
        finish();  // Close the activity in case of an error
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();  // Resume the GLSurfaceView
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();  // Pause the GLSurfaceView
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (gameRenderer != null) {
            gameRenderer.cleanup();  // Cleanup the renderer on destroy
        }
    }
}