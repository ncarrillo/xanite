package com.example.xemu;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.SearchView;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.PopupMenu;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ShowGameActivity extends AppCompatActivity {

    private static final String TAG = "ShowGameActivity";
    public static final String EXTRA_ENCRYPTED_FOLDER_PATH = "encryptedFolderPath";
    public static final String EXTRA_GAME_URI = "GAME_URI";
    private static final String SHARED_PREFS_NAME = "AppState";
    private static final String KEY_IS_IN_SHOW_GAME_ACTIVITY = "isInShowGameActivity";

    private RecyclerView gameRecyclerView;
    private SearchView gameSearchView;
    private ProgressBar progressBar;
    private GameAdapter gameAdapter;
    private TextView emptyView;

    private ArrayList<String> gameNamesList;
    private HashMap<String, String> gamePathsMap;
    private String folderPath;
    private ExecutorService executorService;

    private GameViewModel gameViewModel;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_show_game);

        setupImmersiveMode();
        initViews();
        setupComponents();
        handleIntentData();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_show_game, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();

        if (id == R.id.action_info) {
            startActivity(new Intent(this, InfoActivity.class));
            return true;
        } else if (id == R.id.action_restart) {
            if (folderPath != null) {
                gameViewModel.loadGames(folderPath);
                progressBar.setVisibility(View.VISIBLE);
            }
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void initViews() {
        gameRecyclerView = findViewById(R.id.game_recycler_view);
        gameSearchView = findViewById(R.id.game_search_view);
        progressBar = findViewById(R.id.progress_bar);
        emptyView = findViewById(R.id.empty_view);
    }

    private void setupImmersiveMode() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );

        decorView.setOnSystemUiVisibilityChangeListener(visibility -> {
            if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                setupImmersiveMode();
            }
        });
    }

    private void setupComponents() {
        gameNamesList = new ArrayList<>();
        gamePathsMap = new HashMap<>();
        gameAdapter = new GameAdapter(this, gameNamesList, this::onGameSelected);
        gameRecyclerView.setLayoutManager(new GridLayoutManager(this, 2));
        gameRecyclerView.setAdapter(gameAdapter);

        gameSearchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                return false;
            }

            @Override
            public boolean onQueryTextChange(String query) {
                gameAdapter.getFilter().filter(query);
                return true;
            }
        });

        gameViewModel = new ViewModelProvider(this).get(GameViewModel.class);
        observeViewModel();
    }

    private void observeViewModel() {
        gameViewModel.getGamesLiveData().observe(this, games -> {
            progressBar.setVisibility(View.GONE);
            if (games.isEmpty()) {
                Toast.makeText(this, R.string.no_games_found, Toast.LENGTH_LONG).show();
            } else {
                gameNamesList.clear();
                gameNamesList.addAll(games.keySet());
                gamePathsMap.clear();
                gamePathsMap.putAll(games);
                gameAdapter.notifyDataSetChanged();
            }
        });

        gameViewModel.getErrorLiveData().observe(this, errorMessage -> {
            progressBar.setVisibility(View.GONE);
            Toast.makeText(this, errorMessage, Toast.LENGTH_LONG).show();
        });
    }

    private void handleIntentData() {
        String encryptedPath = getIntent().getStringExtra(EXTRA_ENCRYPTED_FOLDER_PATH);
        if (encryptedPath == null || encryptedPath.isEmpty()) {
            showErrorAndFinish("Invalid folder path");
            return;
        }

        folderPath = decryptFilePath(encryptedPath);
        if (folderPath.isEmpty()) {
            showErrorAndFinish("Decryption failed");
            return;
        }

        gameViewModel.loadGames(folderPath);
        progressBar.setVisibility(View.VISIBLE);
    }

    private void onGameSelected(String selectedGame) {
        String gameUriString = gamePathsMap.get(selectedGame);

        if (gameUriString != null) {
            Uri gameUri = Uri.parse(gameUriString);
            startActivity(new Intent(this, GamePlayActivity.class)
                    .putExtra(EXTRA_GAME_URI, gameUri));
        } else {
            Toast.makeText(this, R.string.error_file_not_found, Toast.LENGTH_SHORT).show();
        }
    }

    private String decryptFilePath(String encrypted) {
        try {
            return new String(
                    Base64.decode(encrypted, Base64.DEFAULT),
                    StandardCharsets.UTF_8
            );
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Decryption error: " + e.getMessage());
            return "";
        }
    }

    private void showErrorAndFinish(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
        finish();
    }

    @Override
    protected void onPause() {
        super.onPause();
        saveAppState();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (executorService != null) {
            executorService.shutdown();
        }
        clearAppState();
    }

    private void saveAppState() {
        SharedPreferences sharedPreferences = getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putBoolean(KEY_IS_IN_SHOW_GAME_ACTIVITY, true);
        editor.apply();
    }

    private void clearAppState() {
        SharedPreferences sharedPreferences = getSharedPreferences(SHARED_PREFS_NAME, MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putBoolean(KEY_IS_IN_SHOW_GAME_ACTIVITY, false);
        editor.apply();
    }

    // دالة لفتح القائمة
    public void openMenu(View view) {
        PopupMenu popupMenu = new PopupMenu(this, view);
        popupMenu.getMenuInflater().inflate(R.menu.menu_show_game, popupMenu.getMenu());

        popupMenu.setOnMenuItemClickListener(item -> {
            int id = item.getItemId();

            if (id == R.id.action_info) {
                startActivity(new Intent(this, InfoActivity.class));
                return true;
            } else if (id == R.id.action_restart) {
                if (folderPath != null) {
                    gameViewModel.loadGames(folderPath);
                    progressBar.setVisibility(View.VISIBLE);
                }
                return true;
            }

            return false;
        });

        popupMenu.show();
    }
}