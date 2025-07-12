package com.xanite.utils

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.InputStream
import java.io.OutputStream

object FileUtils {

    fun getGamesDirectory(context: Context, folderName: String): File {
        return File(context.getExternalFilesDir(null), folderName).apply {
            if (!exists()) {
                mkdirs()
            }
        }
    }

    fun getFileNameFromUri(context: Context, uri: Uri): String? {
        return when (uri.scheme) {
            "content" -> getContentFileName(context, uri)
            "file" -> uri.lastPathSegment?.let { File(it).name }
            else -> null
        }
    }

    fun copyStreamToFile(inputStream: InputStream, destFile: File): Boolean {
        return try {
            inputStream.use { input ->
                destFile.outputStream().use { output ->
                    input.copyTo(output)
                    true
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }

    private fun getContentFileName(context: Context, uri: Uri): String? {
        return context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                cursor.getString(cursor.getColumnIndexOrThrow(OpenableColumns.DISPLAY_NAME))
            } else {
                null
            }
        }
    }

    fun copyFile(sourceFile: File, destFile: File): Boolean {
        return try {
            sourceFile.inputStream().use { input ->
                destFile.outputStream().use { output ->
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