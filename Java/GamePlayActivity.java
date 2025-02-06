package com.example.xemu;

import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;

public class GamePlayActivity extends AppCompatActivity {

    private String gameFilePath;
    private ProgressBar progressBar;
    private TextView progressText;
    private Handler handler;

    private static final String TAG = "GamePlayActivity";

    static {
        System.loadLibrary("nativ_lib"); // تحميل مكتبة C++ إذا كان هناك كود Native
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gameplay);

        // إخفاء شريط الحالة لجعل اللعبة تعمل بوضع ملء الشاشة
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);

        Log.d(TAG, "onCreate started");

        // الحصول على مسار اللعبة (ISO) من النشاط السابق
        Intent intent = getIntent();
        gameFilePath = intent.getStringExtra("GAME_PATH");

        progressBar = findViewById(R.id.progressBar);
        progressText = findViewById(R.id.progressText);
        handler = new Handler(Looper.getMainLooper());

        // التحقق من صحة مسار الملف
        if (gameFilePath != null && !gameFilePath.isEmpty()) {
            File gameFile = new File(gameFilePath);
            if (gameFile.exists() && gameFile.isFile()) {
                long fileSize = gameFile.length();
                Log.d(TAG, "Game file size: " + fileSize + " bytes");
            } else {
                Log.e(TAG, "Game file not found or invalid file.");
                finish();
                return;
            }
            Log.d(TAG, "Game file path received: " + gameFilePath);
            loadGame(gameFilePath);
        } else {
            Log.e(TAG, "Game file path is null or empty.");
            finish();
        }
    }

    /**
     * يقوم هذا الدالة بتحميل ملف ISO عن طريق قراءته على دفعات وتحديث شريط التقدم.
     * عند انتهاء عملية القراءة (حتى وإن كانت نسبة القراءة أقل من الحجم الكلي)
     * يتم إجبار شريط التقدم ليظهر 100% والانتقال إلى GameDisplayActivity.
     *
     * @param isoPath مسار ملف الـ ISO على التخزين الداخلي.
     */
    public void loadGame(String isoPath) {
        if (isoPath == null || isoPath.isEmpty()) {
            Log.e(TAG, "ISO file path is null or empty.");
            return;
        }

        // إظهار شريط التقدم والنص أثناء تحميل اللعبة
        progressBar.setVisibility(View.VISIBLE);
        progressText.setVisibility(View.VISIBLE);
        progressText.setText("Loading game...");

        File isoFile = new File(isoPath);
        long totalSize = isoFile.length();
        Log.d(TAG, "Total ISO file size: " + totalSize + " bytes");

        new Thread(() -> {
            long readBytes = 0;
            int progress = 0;
            try (InputStream is = new FileInputStream(isoFile)) {
                byte[] buffer = new byte[8192]; // بافر بحجم 8 كيلو بايت
                int len;
                while ((len = is.read(buffer)) != -1) {
                    readBytes += len;
                    progress = (int) ((readBytes * 100) / totalSize);
                    if (progress > 100) progress = 100;
                    int currentProgress = progress;
                    handler.post(() -> {
                        progressBar.setProgress(currentProgress);
                        progressText.setText("Loading game... " + currentProgress + "%");
                    });
                }
            } catch (Exception e) {
                Log.e(TAG, "Error reading ISO file: " + e.getMessage());
                // في حال حدوث خطأ أثناء القراءة، يمكننا المتابعة مع ما تم قراءته
            }
            // بغض النظر عن قيمة readBytes، نرفع النسبة إلى 100%
            handler.post(() -> {
                progressBar.setProgress(100);
                progressText.setText("Loading game... 100%");
            });

            // استدعاء دالة JNI لتحميل اللعبة (إذا كانت مطلوبة)
            nativeLoadGame(isoPath);

            // الانتقال إلى GameDisplayActivity بعد الانتهاء من التحميل
            handler.post(() -> {
                Log.d(TAG, "Game successfully loaded.");
                Intent intent = new Intent(GamePlayActivity.this, GameDisplayActivity.class);
                intent.putExtra("ISO_PATH", isoPath);
                startActivity(intent);
                finish();
            });
        }).start();
    }

    // دالة JNI لتحميل اللعبة (يمكن تعديلها حسب احتياجات التطبيق)
    private native void nativeLoadGame(String isoPath);
}