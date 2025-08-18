package com.xanite.xboxoriginal

import android.content.Context
import android.net.Uri
import com.xanite.models.Game
import com.xanite.utils.FileUtils
import com.xanite.utils.ZipUtils
import java.io.File
import java.io.IOException

class XboxOriginalRepository(private val context: Context) {
    
    private val gamesDir = FileUtils.getGamesDirectory(context, "xbox_original")
    
 
    private val supportedExtensions = listOf("xbe", "iso", "xiso", "zip")
    private val supportedArchiveExtensions = listOf("zip")

    fun getGames(): List<Game> {
        return gamesDir.listFiles()?.flatMap { file ->
            when {
                
                file.extension.lowercase() in supportedArchiveExtensions -> {
                    handleArchiveFile(file)
                }
              
                file.extension.lowercase() in supportedExtensions -> {
                    listOf(createXboxOriginalGame(file))
                }
          
                file.extension.isEmpty() && isLikelyXbeFile(file) -> {
                    listOf(createXboxOriginalGame(file))
                }
                else -> emptyList()
            }
        } ?: emptyList()
    }

    private fun createXboxOriginalGame(file: File): Game {
        return Game(
            path = file.absolutePath,
            name = extractGameName(file.nameWithoutExtension),
            type = when (file.extension.lowercase()) {
                "xbe" -> "XBE"
                "iso" -> "ISO"
                "xiso" -> "XISO"
                else -> "UNKNOWN"
            }
        )
    }

    private fun extractGameName(fileName: String): String {
       
        return fileName
            .replace(Regex("""\[.*?\]|\(.*?\)"""), "") 
            .replace(Regex("""_"""), " ") 
            .replace(Regex("""(?i)\b(ntsc|pal|usa|eur|jap|demo|final|repack|xbox)\b"""), "") 
            .trim()
    }

    private fun handleArchiveFile(file: File): List<Game> {
        val extractedDir = File(gamesDir, file.nameWithoutExtension)
        if (!extractedDir.exists()) {
            if (!ZipUtils.extractZip(file, extractedDir)) {
                return emptyList()
            }
        }
        
      
        return extractedDir.walk()
            .filter { it.isFile && it.extension.lowercase() in supportedExtensions }
            .map { gameFile ->
                createXboxOriginalGame(gameFile)
            }
            .toList()
    }

    fun addGame(uri: Uri): Boolean {
        return try {
            val fileName = FileUtils.getFileNameFromUri(context, uri) ?: return false
            val extension = fileName.substringAfterLast('.', "").lowercase()
            
            
            if (extension !in supportedExtensions) {
                
                if (!isLikelyXbeFile(uri)) {
                    return false
                }
            }
            
            val destFile = File(gamesDir, fileName)
            
            when (extension) {
                in supportedArchiveExtensions -> {
                    val inputStream = context.contentResolver.openInputStream(uri) ?: return false
                    FileUtils.copyStreamToFile(inputStream, destFile)
                    
                    
                    handleArchiveFile(destFile).isNotEmpty()
                }
                else -> {
                    val inputStream = context.contentResolver.openInputStream(uri) ?: return false
                    FileUtils.copyStreamToFile(inputStream, destFile)
                    true
                }
            }
        } catch (e: IOException) {
            e.printStackTrace()
            false
        }
    }
    
    private fun isLikelyXbeFile(uri: Uri): Boolean {
        return try {
            val inputStream = context.contentResolver.openInputStream(uri) ?: return false
            val buffer = ByteArray(4)
            val bytesRead = inputStream.read(buffer)
            inputStream.close()
            
            
            if (bytesRead >= 4) {
                val signature = (buffer[3].toInt() shl 24) or 
                               (buffer[2].toInt() shl 16) or 
                               (buffer[1].toInt() shl 8) or 
                               buffer[0].toInt()
                return signature == 0x48454258 
            }
            false
        } catch (e: Exception) {
            false
        }
    }
    
    private fun isLikelyXbeFile(file: File): Boolean {
        return try {
            val inputStream = file.inputStream()
            val buffer = ByteArray(4)
            val bytesRead = inputStream.read(buffer)
            inputStream.close()
              
            if (bytesRead >= 4) {
                val signature = (buffer[3].toInt() shl 24) or 
                               (buffer[2].toInt() shl 16) or 
                               (buffer[1].toInt() shl 8) or 
                               buffer[0].toInt()
                return signature == 0x48454258 
            }
            false
        } catch (e: Exception) {
            false
        }
    }
}