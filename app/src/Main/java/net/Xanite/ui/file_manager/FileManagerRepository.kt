package com.xanite.filemanager

import android.content.Context
import java.io.File

class FileManagerRepository(private val context: Context) {
    fun getFiles(directory: String): List<File> {
        val dir = if (directory.isEmpty()) {
            getAppRootDirectory()
        } else {
            File(directory).takeIf { it.exists() } ?: return emptyList()
        }
        
        return dir.listFiles()?.filter { 
            !it.name.startsWith(".")
        }?.sortedWith(compareBy({ !it.isDirectory }, { it.name.toLowerCase() })) 
            ?: emptyList()
    }

    private fun getAppRootDirectory(): File {
        return context.filesDir.parentFile ?: context.filesDir
    }
}