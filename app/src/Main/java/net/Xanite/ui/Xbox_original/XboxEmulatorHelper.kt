package com.xanite.xboxoriginal

import android.content.Context
import android.content.pm.PackageManager

object XboxEmulatorHelper {
    fun isXemuInstalled(context: Context): Boolean {
        return try {
            context.packageManager.getPackageInfo("com.xemu.app", PackageManager.GET_ACTIVITIES)
            true
        } catch (e: PackageManager.NameNotFoundException) {
            false
        }
    }
}