package com.example.xemu;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    // تحميل المكتبة الأصلية Xemu
    static {
        System.loadLibrary("Xemu_lib"); // اسم المكتبة بدون الامتداد ".so"
    }

    // تعريف الدوال JNI للتفاعل مع C++
    public native void initializeEmulator(); // دالة لتشغيل المحاكي
    public native void loadPackages(); // دالة لتحميل الحزم

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // تطبيق وضع الشاشة الكاملة
        hideSystemUI();

        // الحصول على زر Next من الواجهة
        Button nextButton = findViewById(R.id.next_button);

        // تعيين حدث الضغط على زر "Next"
        nextButton.setOnClickListener(v -> {
            // استدعاء الدالة من C++ عند الضغط على زر Next
            initializeEmulator();
            loadPackages();
            Toast.makeText(MainActivity.this, "Emulator Start!", Toast.LENGTH_SHORT).show();

            // الانتقال إلى BiosActivity
            Intent intent = new Intent(MainActivity.this, BiosActivity.class);
            startActivity(intent); // بدء النشاط الجديد
        });
    }

    // دالة لتفعيل وضع الشاشة الكاملة
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

    // إعادة تفعيل وضع الشاشة الكاملة عند عودة التركيز للنشاط
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }
}