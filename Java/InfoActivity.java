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
        setContentView(R.layout.activity_info);

        setupDeveloperLinks();
        setupDonationButton();
        setupBackButton();
        setupSocialLinks();
    }

    private void setupDeveloperLinks() {
        ImageButton btnAli = findViewById(R.id.btn_ali);
        ImageButton btnYebot = findViewById(R.id.btn_yebot);

        btnAli.setOnClickListener(v -> openUrl("https://github.com/dev-Ali2008"));
        btnYebot.setOnClickListener(v -> openUrl("https://github.com/Yebot32"));
    }

    private void setupSocialLinks() {
        
        ImageButton btnDiscord = findViewById(R.id.btn_discord);
        btnDiscord.setOnClickListener(v -> openUrl("https://discord.gg/7AdXU5tU"));

        ImageButton btnTelegram = findViewById(R.id.btn_telegram);
        btnTelegram.setOnClickListener(v -> openUrl("https://t.me/xemu_android"));

        ImageButton btnYouTube = findViewById(R.id.btn_youtube);
        btnYouTube.setOnClickListener(v -> openUrl("https://youtube.com/@emulator_xemu?si=9i2jErGrik5tbDNo"));

        ImageButton btnGitHub = findViewById(R.id.btn_github);
        btnGitHub.setOnClickListener(v -> openUrl("https://github.com/dev-Ali2008/Xemu-android"));
    }

    private void setupDonationButton() {
        Button btnDonate = findViewById(R.id.btn_donate);
        btnDonate.setOnClickListener(v -> openUrl("https://www.patreon.com/c/Xemu_android"));
    }

    private void setupBackButton() {
        Button backButton = findViewById(R.id.back_button);
        backButton.setOnClickListener(v -> onBackPressed());
    }

    private void openUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        startActivity(intent);
    }
}