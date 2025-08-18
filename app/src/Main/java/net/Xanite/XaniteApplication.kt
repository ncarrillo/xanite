package com.xanite

import android.app.Application
import android.os.Process

class XaniteApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        
        try {
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}