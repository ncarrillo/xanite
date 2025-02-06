
package com.example.xemu;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Button;
import android.widget.ImageButton;
import androidx.appcompat.app.AppCompatActivity;

public class InfoActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_info); // تحميل واجهة activity_info.xml

        // إعداد الروابط والأزرار
        setupDeveloperLinks(); // إضافة هذا السطر
        setupDonationButton();
        setupBackButton();
    }

    /**
     * إعداد روابط المطورين لفتح روابط GitHub.
     */
    private void setupDeveloperLinks() {
        ImageButton btnAli = findViewById(R.id.btn_ali);
        ImageButton btnYebot = findViewById(R.id.btn_yebot);

        btnAli.setOnClickListener(v -> openUrl("https://github.com/dev-Ali2008"));
        btnYebot.setOnClickListener(v -> openUrl("https://github.com/Yebot32"));
    }

    /**
     * إعداد زر التبرع لفتح رابط Patreon.
     */
    private void setupDonationButton() {
        Button btnDonate = findViewById(R.id.btn_donate);
        btnDonate.setOnClickListener(v -> openUrl("https://www.patreon.com/c/Xemu_android"));
    }

    /**
     * إعداد زر العودة لإغلاق النشاط الحالي.
     */
    private void setupBackButton() {
        Button backButton = findViewById(R.id.back_button);
        backButton.setOnClickListener(v -> onBackPressed());
    }

    /**
     * فتح رابط خارجي في متصفح الجهاز.
     *
     * @param url الرابط المراد فتحه.
     */
    private void openUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        startActivity(intent);
    }
}