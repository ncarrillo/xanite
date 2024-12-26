package com.example.xemu;

import android.content.Context;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.Surface;
import android.opengl.Matrix;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GameRenderer implements GLSurfaceView.Renderer {

    private Surface surface;
    private String isoFilePath;
    private String biosFilePath;
    private String mcpxFilePath;
    private String hddImageFilePath;
    private Emulator emulator;
    private Context context;

    private float[] mvpMatrix = new float[16];
    private float[] projectionMatrix = new float[16];
    private float[] viewMatrix = new float[16];

    private int shaderProgram;
    private int vertexBufferObject;
    private int vertexArrayObject;
    private int textureId;

    private boolean vsyncEnabled = true;

    private long lastTime = 0;
    private int frameCount = 0;
    private float fps = 0;

    private boolean isGameStarted = false;

    // GPU and rendering settings
    private boolean accuracy = false;
    private String gpuApi = "OpenGL_ES_3.0";
    private String gpu = "Unknown GPU";
    private String windowAdapting = "Nearest Neighbor";
    private boolean diskShaderCache = true;
    private boolean asynchronousShaders = true;
    private boolean reactiveFlushing = true;

    // Xemu settings
    private boolean isXemuRunning = false;
    private String xemuBiosPath;
    private String xemuDiskImagePath;

    // Constructor
    public GameRenderer(Context context, Surface surface, String isoFilePath, String biosFilePath, String mcpxFilePath, String hddImageFilePath) {
        this.context = context;
        this.surface = surface;
        this.isoFilePath = isoFilePath;  // سيتم تمريره عبر Intent
        this.biosFilePath = biosFilePath;
        this.mcpxFilePath = mcpxFilePath;
        this.hddImageFilePath = hddImageFilePath;
        this.emulator = new Emulator(context, isoFilePath, biosFilePath, mcpxFilePath, hddImageFilePath);

        detectGPU();
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        Log.d("GameRenderer", "Surface created, initializing OpenGL...");

        GLES30.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GLES30.glEnable(GLES30.GL_DEPTH_TEST);
        GLES30.glEnable(GLES30.GL_CULL_FACE);
        GLES30.glEnable(GLES30.GL_BLEND);
        GLES30.glBlendFunc(GLES30.GL_SRC_ALPHA, GLES30.GL_ONE_MINUS_SRC_ALPHA);

        initializeShaders();
        initializeBuffers();
        initializeTextures();
        initializeEmulator();

        Matrix.setIdentityM(mvpMatrix, 0);
        Matrix.setLookAtM(viewMatrix, 0, 0, 0, -5, 0, 0, 0, 0, 1, 0);
        Matrix.frustumM(projectionMatrix, 0, -1, 1, -1, 1, 3, 7);

        loadISOFromIntent(); // تحميل ISO من الـIntent
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        long currentTime = System.nanoTime();
        long elapsedTime = currentTime - lastTime;

        if (elapsedTime >= 1000000000) {
            fps = frameCount / (elapsedTime / 1000000000.0f);
            lastTime = currentTime;
            frameCount = 0;
        }

        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);

        if (!isGameStarted) {
            emulator.start();
            isGameStarted = true;
        }

        renderGameFrame();
        frameCount++;

        drawFPS(fps);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        GLES30.glViewport(0, 0, width, height);
        float ratio = (float) width / height;
        Matrix.frustumM(projectionMatrix, 0, -ratio, ratio, -1, 1, 3, 7);
    }

    private void renderGameFrame() {
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, textureId);
        GLES30.glUseProgram(shaderProgram);
        GLES30.glBindVertexArray(vertexArrayObject);
        GLES30.glDrawArrays(GLES30.GL_TRIANGLES, 0, 6);
    }

    private void drawFPS(float fps) {
        Log.d("FPS", "FPS: " + fps);
    }

    private void initializeShaders() {
        String vertexShaderCode =
                "attribute vec4 vPosition;" +
                "attribute vec2 aTexCoord;" +
                "varying vec2 vTexCoord;" +
                "uniform mat4 uMVPMatrix;" +
                "void main() {" +
                "  gl_Position = uMVPMatrix * vPosition;" +
                "  vTexCoord = aTexCoord;" +
                "}";

        String fragmentShaderCode =
                "precision mediump float;" +
                "varying vec2 vTexCoord;" +
                "uniform sampler2D sTexture;" +
                "void main() {" +
                "  gl_FragColor = texture2D(sTexture, vTexCoord);" +
                "}";

        int vertexShader = loadShader(GLES30.GL_VERTEX_SHADER, vertexShaderCode);
        int fragmentShader = loadShader(GLES30.GL_FRAGMENT_SHADER, fragmentShaderCode);

        shaderProgram = GLES30.glCreateProgram();
        GLES30.glAttachShader(shaderProgram, vertexShader);
        GLES30.glAttachShader(shaderProgram, fragmentShader);
        GLES30.glLinkProgram(shaderProgram);
    }

    private int loadShader(int type, String shaderCode) {
        int shader = GLES30.glCreateShader(type);
        GLES30.glShaderSource(shader, shaderCode);
        GLES30.glCompileShader(shader);

        int[] compileStatus = new int[1];
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, compileStatus, 0);
        if (compileStatus[0] == 0) {
            String log = GLES30.glGetShaderInfoLog(shader);
            Log.e("Shader Compilation", "Failed to compile shader: " + log);
            GLES30.glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    private void initializeBuffers() {
        // Initialize buffers (if needed) for vertex data, index data, etc.
    }

    private void initializeTextures() {
        // Initialize textures as required.
    }

    private void detectGPU() {
        String gpuInfo = android.os.Build.MODEL;
        if (gpuInfo.contains("Snapdragon")) {
            gpu = "Qualcomm Snapdragon";
            gpuApi = "OpenGL_ES_3.0";
        } else if (gpuInfo.contains("Mali")) {
            gpu = "Mali GPU";
            gpuApi = "OpenGL_ES_3.0";
        } else {
            gpu = "Unknown GPU";
            gpuApi = "OpenGL_ES_3.0";
        }
        Log.d("GPU Info", "Detected GPU: " + gpu);
    }

    private void initializeEmulator() {
        if (emulator == null) {
            Log.e("GameRenderer", "Emulator is not initialized properly.");
        }
    }

    private void loadISOFromIntent() {
        // الحصول على مسار ISO من الـIntent
        if (isoFilePath != null && !isoFilePath.isEmpty()) {
            Log.d("GameRenderer", "Loading ISO from: " + isoFilePath);
            emulator.loadISO(isoFilePath);
        } else {
            Log.e("GameRenderer", "ISO path is empty or invalid.");
        }
    }

    // Setters for GPU and rendering settings
    public void setDiskShaderCache(boolean diskShaderCache) {
        this.diskShaderCache = diskShaderCache;
    }

    public void setAsynchronousShaders(boolean asynchronousShaders) {
        this.asynchronousShaders = asynchronousShaders;
    }

    public void setReactiveFlushing(boolean reactiveFlushing) {
        this.reactiveFlushing = reactiveFlushing;
    }

    // Method to change GPU API settings dynamically
    public void setGpuApi(String gpuApi) {
        this.gpuApi = gpuApi;
        Log.d("GameRenderer", "GPU API set to: " + gpuApi);
    }

    // Method to change window adaptation strategy (e.g., for scaling)
    public void setWindowAdapting(String windowAdapting) {
        this.windowAdapting = windowAdapting;
        Log.d("GameRenderer", "Window adapting set to: " + windowAdapting);
    }

    // Method to start the emulator with a fresh start
    public void restartEmulator() {
        emulator.restart();
        Log.d("GameRenderer", "Emulator restarted.");
    }

    // Method to stop the emulator if running
    public void stopEmulator() {
        emulator.stop();
        Log.d("GameRenderer", "Emulator stopped.");
    }

    // Method to adjust the frame rate control (V-Sync)
    public void setVsyncEnabled(boolean enabled) {
        this.vsyncEnabled = enabled;
        Log.d("GameRenderer", "V-Sync enabled: " + vsyncEnabled);
    }

    // Getter for frame rate (FPS)
    public float getFps() {
        return fps;
    }

    // Set accuracy mode (for high precision rendering)
    public void setAccuracy(boolean accuracy) {
        this.accuracy = accuracy;
        if (accuracy) {
            Log.d("GameRenderer", "Accuracy mode enabled.");
        } else {
            Log.d("GameRenderer", "Accuracy mode disabled.");
        }
    }

    // Method to set the GPU dynamically
    public void setGpu(String gpu) {
        this.gpu = gpu;
        Log.d("GameRenderer", "GPU set to: " + gpu);
    }
}