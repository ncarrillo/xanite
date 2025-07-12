package com.xanite.utils

import android.content.Context
import android.content.SharedPreferences

object GeneralSettings {
    private lateinit var prefs: SharedPreferences

    fun initialize(context: Context) {
        prefs = context.getSharedPreferences("XanitePrefs", Context.MODE_PRIVATE)
    }

    fun getBoolean(key: String, default: Boolean): Boolean {
        return prefs.getBoolean(key, default)
    }

    fun getInt(key: String, default: Int): Int {
        return prefs.getInt(key, default)
    }

    fun setValue(key: String, value: Any?) {
        when (value) {
            is Boolean -> prefs.edit().putBoolean(key, value).apply()
            is Int -> prefs.edit().putInt(key, value).apply()
            is String -> prefs.edit().putString(key, value).apply()
            null -> prefs.edit().remove(key).apply()
        }
    }

    inline fun boolean(key: String, default: Boolean) = getBoolean(key, default)

    operator fun get(key: String): Any? {
        return when {
            prefs.contains(key) -> when {
                prefs.all[key] is Boolean -> getBoolean(key, false)
                prefs.all[key] is Int -> getInt(key, 0)
                prefs.all[key] is String -> prefs.getString(key, null)
                else -> null
            }
            else -> null
        }
    }

    fun getString(key: String, default: String? = null): String? {
        return prefs.getString(key, default)
    }
}