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
        if (isExternalStorageWritable()) {
            File logDirectory = getLogDirectory();

            if (logDirectory != null && createLogDirectoryIfNotExists(logDirectory)) {
                File logFile = new File(logDirectory, LOG_FILE_NAME);
                writeToFile(logFile, message);
            } else {
                Log.e(TAG, "Failed to access or create log directory.");
            }
        } else {
            Log.e(TAG, "External storage is not available or not writable.");
        }
    }


    private static boolean isExternalStorageWritable() {
        return Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED);
    }

    private static File getLogDirectory() {
        return new File(Environment.getExternalStorageDirectory(), LOG_FOLDER_NAME);
    }

    private static boolean createLogDirectoryIfNotExists(File logDirectory) {
        if (!logDirectory.exists()) {
            if (logDirectory.mkdirs()) {
                Log.d(TAG, "Log directory created: " + logDirectory.getAbsolutePath());
                return true;
            } else {
                Log.e(TAG, "Failed to create log directory.");
                return false;
            }
        }
        return true;
    }

    private static void writeToFile(File logFile, String message) {
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
    }
}