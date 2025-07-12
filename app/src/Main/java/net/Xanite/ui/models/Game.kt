package com.xanite.models

import java.io.File

data class Game(
    val path: String,
    val name: String?,
    val type: String = "",
    val coverPath: String? = null,
    val lastPlayed: Long = 0
) {
    fun getGameFile(): File = File(path)

    val fileSize: Long
        get() = getGameFile().length()

    fun getSizeInGB(): Double {
        return fileSize.toDouble() / (1024 * 1024 * 1024)
    }

    fun getFormattedSize(): String {
        return "%.2f GB".format(getSizeInGB())
    }
}