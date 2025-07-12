package com.xanite.utils

import java.io.File
import java.io.FileOutputStream
import java.util.zip.ZipFile

object ZipUtils {
    fun extractZip(zipFile: File, destination: File): Boolean {
        return try {
            if (!destination.exists()) {
                destination.mkdirs()
            }

            ZipFile(zipFile).use { zip ->
                zip.entries().asSequence().forEach { entry ->
                    val outputFile = File(destination, entry.name)
                    if (entry.isDirectory) {
                        outputFile.mkdirs()
                    } else {
                        outputFile.parentFile?.mkdirs()
                        zip.getInputStream(entry).use { input ->
                            FileOutputStream(outputFile).use { output ->
                                input.copyTo(output)
                            }
                        }
                    }
                }
            }
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
}