package com.example.xemu;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.lifecycle.ViewModelProvider;

import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class GamePlayActivity extends AppCompatActivity {

    private static final String TAG = "GamePlayActivity";
    public static final String EXTRA_GAME_URI = "GAME_URI";

    private ProgressBar progressBar;
    private TextView progressText;
    private ExecutorService executorService;
    private GamePlayViewModel gamePlayViewModel;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gameplay);

        setupFullscreen();
        initViews();
        initViewModel();

        Uri gameUri = getIntent().getParcelableExtra(EXTRA_GAME_URI);
        if (gameUri == null) {
            showErrorAndFinish("Invalid game URI");
            return;
        }

        startFileProcessing(gameUri);
    }

    private void setupFullscreen() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    private void initViews() {
        progressBar = findViewById(R.id.progressBar);
        progressText = findViewById(R.id.progressText);
        progressBar.setMax(100);
    }

    private void initViewModel() {
        gamePlayViewModel = new ViewModelProvider(this).get(GamePlayViewModel.class);
        gamePlayViewModel.getProgressLiveData().observe(this, progress -> {
            progressBar.setProgress(progress);
            progressText.setText(String.format(Locale.getDefault(), "%d%%", progress));
        });

        gamePlayViewModel.getErrorLiveData().observe(this, errorMessage -> {
            Toast.makeText(this, errorMessage, Toast.LENGTH_LONG).show();
            finish();
        });

        gamePlayViewModel.getSuccessLiveData().observe(this, isoUri -> {
            startActivity(new Intent(this, GameDisplayActivity.class)
                    .putExtra("ISO_URI", isoUri.toString()));
            finish();
        });
    }

    private void startFileProcessing(Uri gameUri) {
        executorService = Executors.newSingleThreadExecutor();
        executorService.execute(() -> gamePlayViewModel.processGameUri(getApplication(), gameUri));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (executorService != null) {
            executorService.shutdown();
        }
    }

    private void showErrorAndFinish(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
        finish();
    }
}