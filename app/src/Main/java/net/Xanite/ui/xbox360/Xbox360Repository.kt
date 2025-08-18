package com.xanite.xbox360

import android.content.Context
import android.net.Uri
import com.xanite.models.Game
import com.xanite.utils.FileUtils
import com.xanite.utils.ZipUtils
import java.io.File
import java.io.IOException

class Xbox360Repository(private val context: Context) {
    private val gamesDir = FileUtils.getGamesDirectory(context, "xbox_360")
    
    private val supportedExtensions = listOf("iso", "xex", "god", "arc", "bin", "zip")
    private val supportedArchiveExtensions = listOf("zip")
    private val xbox360FolderPattern = Regex("[0-9A-Fa-f]{8}")

    fun getGames(): List<Game> {
        return gamesDir.listFiles()?.flatMap { file ->
            when {
              
                file.isDirectory && file.name.matches(xbox360FolderPattern) -> {
                    val gameFiles = file.walk()
                        .filter { it.isFile && it.parentFile?.name == "000D0000" }
                        .toList()
                    
                    if (gameFiles.isNotEmpty()) {
                        listOf(createXbox360Game(file, gameFiles.first()))
                    } else {
                        emptyList()
                    }
                }
                
                file.extension.lowercase() in supportedArchiveExtensions -> {
                    handleArchiveFile(file)
                }
              
                file.extension.lowercase() in supportedExtensions -> {
                    listOf(Game(
                        path = file.absolutePath,
                        name = file.nameWithoutExtension,
                        type = file.extension.uppercase()
                    ))
                }
                else -> emptyList()
            }
        } ?: emptyList()
    }

    private fun createXbox360Game(folder: File, gameFile: File): Game {
        
        val originalFolderName = folder.parentFile?.name?.removeSuffix(".zip") ?: folder.name
        return Game(
            path = gameFile.absolutePath,
            name = extractGameName(originalFolderName),
            type = "XBOX360"
        )
    }

    private fun extractGameName(folderName: String): String {
        // تحسين اسم اللعبة من اسم الملف الأصلي
        return folderName
            .replace(Regex("""\[.*?\]|\(.*?\)"""), "") // إزالة الأقواس
            .replace(Regex("""^\s*-|\s*-\s*$"""), "") // إزالة الواصلات
            .trim()
    }

    private fun handleArchiveFile(file: File): List<Game> {
        val extractedDir = File(gamesDir, file.nameWithoutExtension)
        if (!extractedDir.exists()) {
            if (!ZipUtils.extractZip(file, extractedDir)) {
                return emptyList()
            }
        }
        
        // البحث عن ملفات الألعاب في المجلد المستخرج
        return extractedDir.listFiles()?.flatMap { innerFile ->
            when {
                innerFile.isDirectory && innerFile.name.matches(xbox360FolderPattern) -> {
                    val gameFiles = innerFile.walk()
                        .filter { it.isFile && it.parentFile?.name == "000D0000" }
                        .toList()
                    
                    gameFiles.map { gameFile ->
                        createXbox360Game(innerFile, gameFile)
                    }
                }
                else -> emptyList()
            }
        } ?: emptyList()
    }

    fun addGame(uri: Uri): Boolean {
        return try {
            val fileName = FileUtils.getFileNameFromUri(context, uri) ?: return false
            val extension = fileName.substringAfterLast('.', "").lowercase()
            
            if (extension !in supportedExtensions) return false
            
            val destFile = File(gamesDir, fileName)
            
            when (extension) {
                in supportedArchiveExtensions -> {
                    val inputStream = context.contentResolver.openInputStream(uri) ?: return false
                    FileUtils.copyStreamToFile(inputStream, destFile)
                    
                    // استخراج الملفات من ZIP
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
}