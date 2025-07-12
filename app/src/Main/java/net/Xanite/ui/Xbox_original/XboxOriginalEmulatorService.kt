package com.xanite.xboxoriginal

import android.app.Service
import android.content.Intent
import android.os.IBinder

class XboxOriginalEmulatorService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val gamePath = intent?.getStringExtra("GAME_PATH")
        return START_NOT_STICKY
    }
}