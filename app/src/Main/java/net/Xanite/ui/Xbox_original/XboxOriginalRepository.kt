package com.xanite.xboxoriginal

import android.content.Context
import android.net.Uri
import com.xanite.models.Game
import com.xanite.utils.FileUtils
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

class XboxOriginalRepository(private val context: Context) {
    private val gamesDir = FileUtils.getGamesDirectory(context, "xbox_original").apply { mkdirs() }
    private val biosDir = File(context.filesDir, "bios").apply { mkdirs() }

    fun getGames(): List<Game> {
        return gamesDir.listFiles()
            ?.filter { it.extension in setOf("iso", "xiso") }
            ?.map { Game(it.absolutePath, it.nameWithoutExtension) } 
            ?: emptyList()
    }

    fun getBiosFiles(): List<String> {
        return biosDir.listFiles()
            ?.filter { it.name.endsWith(".bin") }
            ?.map { it.name }
            ?: emptyList()
    }

    fun addGame(uri: Uri): Boolean {
        return try {
            context.contentResolver.openInputStream(uri)?.use { input ->
                val fileName = FileUtils.getFileNameFromUri(context, uri) ?: return false
                val destFile = File(gamesDir, fileName)
                FileUtils.copyStreamToFile(input, destFile)
                true
            } ?: false
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }

    fun copyBiosFromAssets(biosName: String): Boolean {
        return try {
            context.assets.open(biosName).use { input ->
                FileOutputStream(File(biosDir, biosName)).use { output ->
                    input.copyTo(output)
                    true
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
}