package com.xanite.xboxoriginal

import android.content.Context
import android.content.pm.PackageManager

object XboxEmulatorHelper {

    init {
        System.loadLibrary("xbox_utils")
        System.loadLibrary("memory_allocator") 
    }

    fun isXemuInstalled(context: Context): Boolean {
        return try {
            context.packageManager.getPackageInfo("com.xemu.app", PackageManager.GET_ACTIVITIES)
            true
        } catch (e: PackageManager.NameNotFoundException) {
            false
        }
    }
}