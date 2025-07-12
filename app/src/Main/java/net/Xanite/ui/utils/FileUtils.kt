// FileUtils.kt
package com.xanite.utils

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.InputStream
import java.io.OutputStream

object FileUtils {
    /**
     * إنشاء أو الحصول على مجلد الألعاب
     * @param context سياق التطبيق
     * @param folderName اسم المجلد المطلوب
     * @return File object للمجلد
     */
    fun getGamesDirectory(context: Context, folderName: String): File {
        return File(context.getExternalFilesDir(null), folderName).apply {
            if (!exists()) {
                mkdirs()
            }
        }
    }

    /**
     * الحصول على اسم الملف من URI
     * @param context سياق التطبيق
     * @param uri URI الملف
     * @return اسم الملف أو null إذا فشل
     */
    fun getFileNameFromUri(context: Context, uri: Uri): String? {
        return when (uri.scheme) {
            "content" -> getContentFileName(context, uri)
            "file" -> uri.lastPathSegment?.let { File(it).name }
            else -> null
        }
    }

    /**
     * نسخ محتوى InputStream إلى ملف
     * @param inputStream InputStream المصدر
     * @param destFile File الهدف
     * @return true إذا نجحت العملية، false إذا فشلت
     */
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

    /**
     * دالة مساعدة للحصول على اسم الملف من Content URI
     */
    private fun getContentFileName(context: Context, uri: Uri): String? {
        return context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                cursor.getString(cursor.getColumnIndexOrThrow(OpenableColumns.DISPLAY_NAME))
            } else {
                null
            }
        }
    }

    /**
     * دالة إضافية لنسخ الملفات
     * @param sourceFile الملف المصدر
     * @param destFile الملف الهدف
     * @return true إذا نجحت العملية
     */
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