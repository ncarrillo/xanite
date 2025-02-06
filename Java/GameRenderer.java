package com.example.xemu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GameRenderer implements GLSurfaceView.Renderer {

    private static final String TAG = "GameRenderer";
    private Context context;
    private String isoFilePath;
    private int textureID;

    static {
        System.loadLibrary("mik_lib"); // تحميل مكتبة C++
    }

    private int program;
    private int vertexShader;
    private int fragmentShader;

    private int[] vbo = new int[1];
    private int[] fbo = new int[1];

    private long lastTime;
    private int frameCount;
    private float fps;

    // 3D Cube vertices and indices
    private final float[] vertices = {
        -0.5f, -0.5f, -0.5f, 
        0.5f, -0.5f, -0.5f,  
        0.5f,  0.5f, -0.5f,  
        -0.5f,  0.5f, -0.5f, 
        -0.5f, -0.5f,  0.5f, 
        0.5f, -0.5f,  0.5f,  
        0.5f,  0.5f,  0.5f,  
        -0.5f,  0.5f,  0.5f  
    };

    private final short[] indices = {
        0, 1, 2, 2, 3, 0, 
        4, 5, 6, 6, 7, 4, 
        0, 1, 5, 5, 4, 0,  
        2, 3, 7, 7, 6, 2,  
        0, 3, 7, 7, 4, 0,  
        1, 2, 6, 6, 5, 1   
    };

    public GameRenderer(Context context) {
        this.context = context;
        lastTime = System.currentTimeMillis();
        frameCount = 0;
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        GLES30.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GLES30.glEnable(GLES30.GL_DEPTH_TEST);
        GLES30.glEnable(GLES30.GL_BLEND);
        GLES30.glBlendFunc(GLES30.GL_SRC_ALPHA, GLES30.GL_ONE_MINUS_SRC_ALPHA);

        // Load shaders
        vertexShader = loadShader(GLES30.GL_VERTEX_SHADER, readShaderFromFile("vertex_shader.glsl"));
        fragmentShader = loadShader(GLES30.GL_FRAGMENT_SHADER, readShaderFromFile("fragment_shader.glsl"));

        // Create shader program
        program = GLES30.glCreateProgram();
        GLES30.glAttachShader(program, vertexShader);
        GLES30.glAttachShader(program, fragmentShader);
        GLES30.glLinkProgram(program);
        GLES30.glUseProgram(program);

        // Generate buffers
        GLES30.glGenBuffers(1, vbo, 0);
        GLES30.glGenFramebuffers(1, fbo, 0);

        String gpuRenderer = GLES30.glGetString(GLES30.GL_RENDERER);
        String gpuVersion = GLES30.glGetString(GLES30.GL_VERSION);
        Log.d(TAG, "GPU Renderer: " + gpuRenderer + ", GPU Version: " + gpuVersion);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        GLES30.glViewport(0, 0, width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);

        // Draw 3D Cube
        draw3DCube();

        // Draw FPS overlay
        drawOverlay();

        // Calculate FPS
        calculateFPS();
    }

    private void draw3DCube() {
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo[0]);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, vertices.length * 4, createFloatBuffer(vertices), GLES30.GL_STATIC_DRAW);

        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, fbo[0]);
        GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, indices.length * 2, createShortBuffer(indices), GLES30.GL_STATIC_DRAW);

        GLES30.glDrawElements(GLES30.GL_TRIANGLES, indices.length, GLES30.GL_UNSIGNED_SHORT, 0);
    }

    private void drawOverlay() {
        Bitmap overlayBitmap = Bitmap.createBitmap(512, 512, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(overlayBitmap);

        Paint paint = new Paint();
        paint.setColor(android.graphics.Color.RED);
        paint.setTextSize(50);
        paint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));

        String fpsText = "FPS: " + String.format("%.2f", fps);
        String gpuText = "GPU: " + detectGPU();
        canvas.drawText(fpsText, 10, 60, paint);
        canvas.drawText(gpuText, 10, 120, paint);
    }

    private void calculateFPS() {
        frameCount++;
        long currentTime = System.currentTimeMillis();
        if (currentTime - lastTime >= 1000) {
            fps = frameCount / ((currentTime - lastTime) / 1000.0f);
            frameCount = 0;
            lastTime = currentTime;
        }
    }

    private String detectGPU() {
        String gpuRenderer = GLES30.glGetString(GLES30.GL_RENDERER);
        if (gpuRenderer.toLowerCase().contains("adreno")) {
            return "Adreno (Snapdragon)";
        } else if (gpuRenderer.toLowerCase().contains("mali")) {
            return "Mali (Valhall)";
        } else {
            return gpuRenderer;
        }
    }

    public void loadISO(String isoFilePath) {
        if (isoFilePath == null || isoFilePath.isEmpty()) {
            Log.e(TAG, "Invalid ISO file path");
            return;
        }

        if (!new File(isoFilePath).exists()) {
            Log.e(TAG, "ISO file not found: " + isoFilePath);
            return;
        }

        this.isoFilePath = isoFilePath;
        Log.d(TAG, "Loading ISO file: " + isoFilePath);

        loadTextureFromISO(isoFilePath);
    }

    private void loadTextureFromISO(String filePath) {
        try {
            FileInputStream fis = new FileInputStream(filePath);
            byte[] header = new byte[256];
            fis.read(header);
            fis.close();

            textureID = createTextureFromData(header);
        } catch (IOException e) {
            Log.e(TAG, "Error reading ISO: " + e.getMessage());
        }
    }

    private int createTextureFromData(byte[] data) {
        int[] textures = new int[1];
        GLES30.glGenTextures(1, textures, 0);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, textures[0]);

        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR);

        ByteBuffer buffer = ByteBuffer.wrap(data);
        GLES30.glTexImage2D(
            GLES30.GL_TEXTURE_2D,
            0,
            GLES30.GL_RGBA,
            64,
            64,
            0,
            GLES30.GL_RGBA,
            GLES30.GL_UNSIGNED_BYTE,
            buffer
        );

        return textures[0];
    }

    private FloatBuffer createFloatBuffer(float[] array) {
        ByteBuffer bb = ByteBuffer.allocateDirect(array.length * 4);
        bb.order(ByteOrder.nativeOrder());
        FloatBuffer buffer = bb.asFloatBuffer();
        buffer.put(array);
        buffer.position(0);
        return buffer;
    }

    private ShortBuffer createShortBuffer(short[] array) {
        ByteBuffer bb = ByteBuffer.allocateDirect(array.length * 2);
        bb.order(ByteOrder.nativeOrder());
        ShortBuffer buffer = bb.asShortBuffer();
        buffer.put(array);
        buffer.position(0);
        return buffer;
    }

    private int loadShader(int type, String shaderCode) {
        int shader = GLES30.glCreateShader(type);
        GLES30.glShaderSource(shader, shaderCode);
        GLES30.glCompileShader(shader);

        int[] compileStatus = new int[1];
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, compileStatus, 0);
        if (compileStatus[0] == GLES30.GL_FALSE) {
            Log.e(TAG, "Shader Compilation failed: " + shaderCode);
        }

        return shader;
    }

    private String readShaderFromFile(String filename) {
        try {
            InputStream is = context.getAssets().open(filename);
            BufferedReader reader = new BufferedReader(new InputStreamReader(is));
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            reader.close();
            return sb.toString();
        } catch (IOException e) {
            Log.e(TAG, "Error reading shader: " + e.getMessage());
            return "";
        }
    }

    public void cleanup() {
        Log.d(TAG, "Cleaning up resources.");
        GLES30.glDeleteBuffers(1, vbo, 0);
        GLES30.glDeleteFramebuffers(1, fbo, 0);
    }
}