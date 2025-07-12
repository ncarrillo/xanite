package com.xanite.utils

import android.content.Context
import android.content.SharedPreferences

object SharedPrefs {
    private const val PREFS_NAME = "XanitePrefs"
    private const val KEY_BIOS_PATH = "xbox_original_bios_path"
    private const val KEY_BIOS_VERSION = "xbox_original_bios_version"
    private const val KEY_BIOS_VALID = "xbox_original_bios_valid"

    // حفظ بيانات BIOS
    fun saveBiosInfo(
        context: Context,
        path: String,
        version: String,
        isValid: Boolean
    ) {
        getPrefs(context).edit().apply {
            putString(KEY_BIOS_PATH, path)
            putString(KEY_BIOS_VERSION, version)
            putBoolean(KEY_BIOS_VALID, isValid)
            apply()
        }
    }

    // جلب بيانات BIOS
    fun getBiosInfo(context: Context): BiosInfo? {
        val prefs = getPrefs(context)
        val path = prefs.getString(KEY_BIOS_PATH, null)
        val version = prefs.getString(KEY_BIOS_VERSION, null)
        
        return if (path != null && version != null) {
            BiosInfo(
                path = path,
                version = version,
                isValid = prefs.getBoolean(KEY_BIOS_VALID, false)
            )
        } else {
            null
        }
    }

    private fun getPrefs(context: Context): SharedPreferences {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }

    data class BiosInfo(
        val path: String,
        val version: String,
        val isValid: Boolean
    )
}