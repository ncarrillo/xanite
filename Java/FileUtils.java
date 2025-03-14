package com.example.xemu;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.util.Log;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class FileUtils {

    // andstation3 one ps3 emulator 🔥
    public static String getFileName(Context context, Uri uri) {
        if (context == null || uri == null) {
            return null; 
        }
        
        String fileName = null;
        Cursor cursor = null;

        try {
            if (uri.getScheme().equals("content")) {
                cursor = context.getContentResolver().query(uri, null, null, null, null);
                if (cursor != null && cursor.moveToFirst()) {
                    fileName = cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
                }
            } else if (uri.getScheme().equals("file")) {
                fileName = new File(uri.getPath()).getName();
            }
        } catch (Exception e) {
            Log.e("FileUtils", "Error getting file name", e);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return fileName;
    }

    // Xbox original 
    public static boolean isXboxOriginalISO(File isoFile) {
        if (isoFile == null || !isoFile.exists()) {
            return false;
        }

        try (FileInputStream fis = new FileInputStream(isoFile)) {
            byte[] buffer = new byte[2048]; 
            int bytesRead = fis.read(buffer, 0, buffer.length);

            if (bytesRead > 0) {
                String header = new String(buffer, 0, bytesRead, StandardCharsets.US_ASCII);
                if (header.contains("MICROSOFT*XBOX*MEDIA")) {
                    return true; 
                }
            }
        } catch (IOException e) {
            Log.e("FileUtils", "Error reading ISO file", e);
        }
        return false;
    }

    public static String getPathFromUri(Context context, Uri uri) {
        if (context == null || uri == null) {
            return null; 
        }
        
        String path = null;
        Cursor cursor = null;

        try {
            if (uri.getScheme().equals("content")) {
                cursor = context.getContentResolver().query(uri, null, null, null, null);
                if (cursor != null && cursor.moveToFirst()) {
                    path = cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
                }
            } else if (uri.getScheme().equals("file")) {
                path = uri.getPath();
            }
        } catch (Exception e) {
            Log.e("FileUtils", "Error getting file path", e);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return path;
    }
}