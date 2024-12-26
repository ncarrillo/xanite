package com.example.xemu;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class GamePlayActivity extends AppCompatActivity {

    private String gameFilePath;
    private ProgressBar progressBar;
    private TextView progressText;
    private Handler handler;

    // تحميل المكتبة الأصلية المكتوبة بـ C++
    static {
        System.loadLibrary("nat_lib"); // تأكد من أنك قد أنشأت مكتبة C++ مع هذا الاسم
    }

    private static final String TAG = "GamePlayActivity"; // تعريف متغير TAG للسجلات

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gameplay);

        // جعل التطبيق في وضع الشاشة الكاملة
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);

        // تسجيل عملية بداية الـ onCreate
        Log.d(TAG, "onCreate started");

        // استلام مسار اللعبة من ShowGameActivity
        Intent intent = getIntent();
        gameFilePath = intent.getStringExtra("GAME_PATH");

        // ربط ProgressBar و TextView
        progressBar = findViewById(R.id.progressBar);
        progressText = findViewById(R.id.progressText);
        handler = new Handler(Looper.getMainLooper());

        if (gameFilePath != null && !gameFilePath.isEmpty()) {
            Log.d(TAG, "Game file path received: " + gameFilePath);
            // تنفيذ تحميل اللعبة
            startGame(gameFilePath);
        } else {
            Log.e(TAG, "Game file path is missing.");
            Toast.makeText(this, "Game file path is missing.", Toast.LENGTH_SHORT).show();
        }
    }

    // دالة لتحميل اللعبة وبدء اللعب
    private void startGame(String isoFilePath) {
        Log.d(TAG, "startGame started with isoFilePath: " + isoFilePath);

        // عرض ProgressBar والنص
        progressBar.setVisibility(View.VISIBLE);
        progressText.setVisibility(View.VISIBLE);

        // محاكاة تحميل اللعبة مع تقدم
        new Thread(() -> {
            boolean isGameLoaded = loadGameWithProgress(isoFilePath);

            handler.post(() -> {
                if (isGameLoaded) {
                    // في حال تم تحميل اللعبة بنجاح، يمكنك بدء اللعب
                    Log.d(TAG, "Game successfully loaded.");
                    Toast.makeText(GamePlayActivity.this, "Game is now running!", Toast.LENGTH_SHORT).show();
                    
                    // نقل المستخدم إلى GameRunActivity بعد التحميل
                    Intent intent = new Intent(GamePlayActivity.this, GameRunActivity.class);
                    intent.putExtra("ISO_PATH", isoFilePath); // إرسال مسار ISO
                    startActivity(intent);
                    finish(); // إنهاء النشاط الحالي بعد الانتقال إلى النشاط الجديد
                    
                } else {
                    Log.e(TAG, "Failed to load game.");
                    Toast.makeText(GamePlayActivity.this, "Failed to load game.", Toast.LENGTH_SHORT).show();
                }
            });
        }).start();
    }

    // دالة لتحميل اللعبة مع محاكاة تقدم (تحميل وهمي)
    private boolean loadGameWithProgress(String isoFilePath) {
        Log.d(TAG, "Loading game with file: " + isoFilePath);

        try {
            // محاكاة تقدم التحميل
            for (int i = 1; i <= 100; i++) {
                Thread.sleep(50); // تأخير بسيط لمحاكاة التحميل

                // تحديث ProgressBar
                int progress = i;
                handler.post(() -> {
                    progressBar.setProgress(progress);
                    progressText.setText(progress + "%");
                });
            }

            return true; // تم التحميل بنجاح
        } catch (InterruptedException e) {
            Log.e(TAG, "Error loading game", e);
            return false; // في حال حدوث خطأ أثناء التحميل
        }
    }

    // دالة استدعاء من C++ عبر JNI
    public native String stringFromJNI(); // استدعاء دالة موجودة في C++
}