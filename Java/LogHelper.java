package com.example.xemu;

import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

public class LogHelper {

    private static final String TAG = "LogHelper";
    private static final String LOG_FOLDER_NAME = "XemuLogs";
    private static final String LOG_FILE_NAME = "logs.txt";

    public static void writeLog(String message) {
        // تأكد من أن التخزين الخارجي متاح
        if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            // حدد مسار المجلد
            File logDirectory = new File(Environment.getExternalStorageDirectory(), LOG_FOLDER_NAME);

            // تأكد من أن المجلد موجود، وإذا لم يكن موجودًا قم بإنشائه
            if (!logDirectory.exists()) {
                if (logDirectory.mkdirs()) {
                    Log.d(TAG, "Log directory created: " + logDirectory.getAbsolutePath());
                } else {
                    Log.e(TAG, "Failed to create log directory.");
                    return;
                }
            }

            // تحديد مسار ملف السجلات
            File logFile = new File(logDirectory, LOG_FILE_NAME);

            // كتابة الرسالة في ملف السجلات
            FileWriter fileWriter = null;
            try {
                fileWriter = new FileWriter(logFile, true);
                fileWriter.append(message).append("\n");
                fileWriter.flush();
                Log.d(TAG, "Log written to file: " + message);
            } catch (IOException e) {
                Log.e(TAG, "Error writing log to file: ", e);
            } finally {
                if (fileWriter != null) {
                    try {
                        fileWriter.close();
                    } catch (IOException e) {
                        Log.e(TAG, "Error closing log file writer: ", e);
                    }
                }
            }
        } else {
            Log.e(TAG, "External storage is not available or not writable.");
        }
    }
}