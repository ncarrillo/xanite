package com.xanite.models

import java.io.File

data class Bios(
    val path: String,
    val version: String,
    val isValid: Boolean = false,
    val lastUsed: Long = System.currentTimeMillis()
) {
    fun getBiosFile(): File = File(path)
    
    fun getSizeInMB(): Double {
        return getBiosFile().length().toDouble() / (1024 * 1024)
    }
}