package com.example.xemu;

import android.content.Context;
import android.net.Uri;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.FloatBuffer;
import android.content.res.AssetFileDescriptor;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class GameRenderer implements GLSurfaceView.Renderer {

    private static final String TAG = "GameRenderer";
    private final Context context;
    private final MemoryPool memoryPool;
    private long lastTime = System.nanoTime();
    private int frames = 0;
    private int fps = 0;
    private float joystickX = 0;
    private float joystickY = 0;
    private int shaderProgram = 0;
    private int vertexArray = 0;
    private int vertexBuffer = 0;
    private int vertexCount = 0;
    private final ExecutorService executor = Executors.newFixedThreadPool(4);
    private boolean isGameLoaded = false;
    private String isoUriString = "";

    static {
        System.loadLibrary("mik_lib");
    }

    public GameRenderer(Context context) {
        if (context == null) {
            throw new IllegalArgumentException("Context cannot be null");
        }
        this.context = context;
        memoryPool = new MemoryPool(1024 * 1024 * 256, 1024 * 1024);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {

        setBackgroundColorBasedOnState("default"); 
        
        GLES30.glEnable(GLES30.GL_DEPTH_TEST);
        checkGLError("onSurfaceCreated");

        try {
            if (!nativeInit()) {
                Log.e(TAG, "Failed to initialize native library");
                checkGLError("nativeInit");
            }
            initShaders();
            initModels();
        } catch (Exception e) {
            Log.e(TAG, "Error in onSurfaceCreated: " + e.getMessage());
            setBackgroundColorBasedOnState("error");
        }
    }

    
    public void setBackgroundColorBasedOnState(String state) {
        switch (state) {
            case "error":
                GLES30.glClearColor(1.0f, 0.0f, 0.0f, 1.0f); 
                break;
            case "success":
                GLES30.glClearColor(0.0f, 1.0f, 0.0f, 1.0f); 
                break;
            default:
                GLES30.glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
                break;
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        try {
            nativeResize(852, 480);
            checkGLError("onSurfaceChanged");
            if (!nativeResize(width, height)) {
                Log.e(TAG, "Failed to resize native view");
                checkGLError("nativeResize");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error in onSurfaceChanged: " + e.getMessage());
        }
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        try {
            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);
            long currentTime = System.nanoTime();
            frames++;
            if (currentTime - lastTime >= 1_000_000_000) {
                fps = frames;
                frames = 0;
                lastTime = currentTime;
            }
            nativeHandleJoystickInput(joystickX, joystickY);
            if (isGameLoaded) {
                drawGameContent();
            }
            checkGLError("onDrawFrame");
        } catch (Exception e) {
            Log.e(TAG, "Error in onDrawFrame: " + e.getMessage());
        }
    }

    private void initShaders() {
        String vertexShaderCode = readShaderFileFromAssets("vertex_shader.glsl");
        if (vertexShaderCode == null) {
            Log.e(TAG, "Failed to read vertex shader from assets");
            return;
        }
        String fragmentShaderCode = readShaderFileFromAssets("fragment_shader.glsl");
        if (fragmentShaderCode == null) {
            Log.e(TAG, "Failed to read fragment shader from assets");
            return;
        }

        int vertexShader = loadShader(GLES30.GL_VERTEX_SHADER, vertexShaderCode);
        if (vertexShader == 0) {
            Log.e(TAG, "Failed to compile vertex shader");
            return;
        }

        int fragmentShader = loadShader(GLES30.GL_FRAGMENT_SHADER, fragmentShaderCode);
        if (fragmentShader == 0) {
            Log.e(TAG, "Failed to compile fragment shader");
            return;
        }

        shaderProgram = GLES30.glCreateProgram();
        if (shaderProgram == 0) {
            Log.e(TAG, "Failed to create shader program");
            return;
        }

        GLES30.glAttachShader(shaderProgram, vertexShader);
        GLES30.glAttachShader(shaderProgram, fragmentShader);
        GLES30.glLinkProgram(shaderProgram);

        int[] linkStatus = new int[1];
        GLES30.glGetProgramiv(shaderProgram, GLES30.GL_LINK_STATUS, linkStatus, 0);
        if (linkStatus[0] != GLES30.GL_TRUE) {
            Log.e(TAG, "Failed to link shader program: " + GLES30.glGetProgramInfoLog(shaderProgram));
            GLES30.glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            return;
        }

        GLES30.glDeleteShader(vertexShader);
        GLES30.glDeleteShader(fragmentShader);

        Log.d(TAG, "Shaders loaded and linked successfully");
    }

    private String readShaderFileFromAssets(String filePath) {
        StringBuilder shaderCode = new StringBuilder();
        InputStream inputStream = null;
        BufferedReader reader = null;
        try {
            inputStream = context.getAssets().open(filePath);
            reader = new BufferedReader(new InputStreamReader(inputStream));
            String line;
            while ((line = reader.readLine()) != null) {
                shaderCode.append(line).append("\n");
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to read shader file from assets: " + e.getMessage());
            return null;
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
                if (inputStream != null) {
                    inputStream.close();
                }
            } catch (IOException e) {
                Log.e(TAG, "Failed to close resources: " + e.getMessage());
            }
        }
        return shaderCode.toString();
    }

    private int loadShader(int type, String shaderCode) {
        int shader = GLES30.glCreateShader(type);
        if (shader == 0) {
            Log.e(TAG, "Failed to create shader");
            return 0;
        }
        GLES30.glShaderSource(shader, shaderCode);
        GLES30.glCompileShader(shader);
        int[] compileStatus = new int[1];
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, compileStatus, 0);
        if (compileStatus[0] != GLES30.GL_TRUE) {
            Log.e(TAG, "Failed to compile shader: " + GLES30.glGetShaderInfoLog(shader));
            GLES30.glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    private void initModels() {
        float[] vertices = {
            0.0f,  0.5f, 0.0f,  0.5f, 1.0f,  0.0f, 0.0f, 1.0f,  
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  
            0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f   
        };

        vertexCount = vertices.length / 8; 

        int[] buffers = new int[1];
        GLES30.glGenBuffers(1, buffers, 0);
        vertexBuffer = buffers[0];
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vertexBuffer);
        FloatBuffer vertexBufferData = FloatBuffer.wrap(vertices);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, vertices.length * 4, vertexBufferData, GLES30.GL_STATIC_DRAW);

        GLES30.glGenVertexArrays(1, buffers, 0);
        vertexArray = buffers[0];
        GLES30.glBindVertexArray(vertexArray);

         GLES30.glEnableVertexAttribArray(0); // vPosition
        GLES30.glVertexAttribPointer(0, 3, GLES30.GL_FLOAT, false, 8 * 4, 0);

        GLES30.glEnableVertexAttribArray(1); // vTexCoord
        GLES30.glVertexAttribPointer(1, 2, GLES30.GL_FLOAT, false, 8 * 4, 3 * 4);

        GLES30.glEnableVertexAttribArray(2); // v normaly :/
        GLES30.glVertexAttribPointer(2, 3, GLES30.GL_FLOAT, false, 8 * 4, 5 * 4);

        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, 0);
        GLES30.glBindVertexArray(0);
    }

    private void drawGameContent() {
        try {
            GLES30.glUseProgram(shaderProgram);

            int modelMatrixLoc = GLES30.glGetUniformLocation(shaderProgram, "modelMatrix");
            int viewMatrixLoc = GLES30.glGetUniformLocation(shaderProgram, "viewMatrix");
            int projectionMatrixLoc = GLES30.glGetUniformLocation(shaderProgram, "projectionMatrix");
            int lightPositionLoc = GLES30.glGetUniformLocation(shaderProgram, "lightPosition");

            float[] modelMatrix = new float[16];
            float[] viewMatrix = new float[16];
            float[] projectionMatrix = new float[16];
            android.opengl.Matrix.setIdentityM(modelMatrix, 0);
            android.opengl.Matrix.setIdentityM(viewMatrix, 0);
            android.opengl.Matrix.setIdentityM(projectionMatrix, 0);

            GLES30.glUniformMatrix4fv(modelMatrixLoc, 1, false, modelMatrix, 0);
            GLES30.glUniformMatrix4fv(viewMatrixLoc, 1, false, viewMatrix, 0);
            GLES30.glUniformMatrix4fv(projectionMatrixLoc, 1, false, projectionMatrix, 0);

            GLES30.glUniform3f(lightPositionLoc, 0.0f, 0.0f, 5.0f);


            GLES30.glBindVertexArray(vertexArray);

            
            GLES30.glDrawArrays(GLES30.GL_TRIANGLES, 0, vertexCount);

            //  VAO
            GLES30.glBindVertexArray(0);
        } catch (Exception e) {
            Log.w(TAG, "Minor rendering issue: " + e.getMessage());
        }
    }

    public boolean loadISO(String isoUriString) {
        if (isoUriString == null || isoUriString.isEmpty()) {
            Log.e(TAG, "Invalid ISO URI");
            return false;
        }
        this.isoUriString = isoUriString;
        Uri isoUri;
        try {
            isoUri = Uri.parse(isoUriString);
        } catch (Exception e) {
            Log.e(TAG, "Invalid URI: " + isoUriString);
            return false;
        }
        executor.submit(() -> {
            try (AssetFileDescriptor fileDescriptor = context.getContentResolver().openAssetFileDescriptor(isoUri, "r");
                 InputStream is = fileDescriptor != null ? fileDescriptor.createInputStream() : null;
                 BufferedInputStream bis = is != null ? new BufferedInputStream(is) : null) {
                if (bis == null) {
                    Log.e(TAG, "Failed to open ISO file");
                    return;
                }
                byte[] buffer = new byte[10 * 1024 * 1024];
                int bytesRead;
                long totalBytesRead = 0;
                while ((bytesRead = bis.read(buffer)) != -1) {
                    if (!nativeLoadISO(buffer, bytesRead)) {
                        Log.e(TAG, "Failed to load ISO data into native library");
                        return;
                    }
                    totalBytesRead += bytesRead;
                    Log.i(TAG, "ISO data chunk loaded successfully (Size: " + totalBytesRead + " bytes)");
                    if (totalBytesRead >= 100 * 1024 * 1024) {
                        break;
                    }
                }
                Log.i(TAG, "Xbox Original ISO loaded successfully. Total bytes: " + totalBytesRead);
                isGameLoaded = true;
            } catch (IOException e) {
                Log.e(TAG, "Error reading ISO from URI: " + e.getMessage());
            }
        });
        return true;
    }

    private void checkGLError(String operation) {
        int error;
        while ((error = GLES30.glGetError()) != GLES30.GL_NO_ERROR) {
            Log.e(TAG, operation + ": OpenGL Error: " + error + " - " + getGLErrorString(error));
        }
    }

    private String getGLErrorString(int error) {
        switch (error) {
            case GLES30.GL_INVALID_ENUM:
                return "GL_INVALID_ENUM: An unacceptable value is specified for an enumerated argument.";
            case GLES30.GL_INVALID_VALUE:
                return "GL_INVALID_VALUE: A numeric argument is out of range.";
            case GLES30.GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION: The specified operation is not allowed in the current state.";
            case GLES30.GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY: There is not enough memory left to execute the command.";
            default:
                return "Unknown error: " + error;
        }
    }

    public void cleanup() {
        try {
            if (shaderProgram != 0) {
                GLES30.glDeleteProgram(shaderProgram);
                shaderProgram = 0;
            }
            if (vertexBuffer != 0) {
                int[] buffers = {vertexBuffer};
                GLES30.glDeleteBuffers(1, buffers, 0);
                vertexBuffer = 0;
            }
            if (vertexArray != 0) {
                int[] arrays = {vertexArray};
                GLES30.glDeleteVertexArrays(1, arrays, 0);
                vertexArray = 0;
            }
            if (!nativeCleanup()) {
                Log.e(TAG, "Failed to cleanup native resources");
            }
            if (memoryPool != null) {
                memoryPool.cleanup();
            }
            executor.shutdown();
        } catch (Exception e) {
            Log.e(TAG, "Error in cleanup: " + e.getMessage());
        }
    }

    public int getFPS() {
        return fps;
    }

    public String getShaderInfo() {
        if (shaderProgram == 0) {
            return "Shader Program: Not Loaded";
        }
        return "Shader Program: " + shaderProgram + ", Vertex Count: " + vertexCount;
    }

    public String getISOInfo() {
        return "ISO URI: " + isoUriString;
    }

    public void handleJoystickInput(float x, float y) {
        joystickX = x;
        joystickY = y;
    }

    public void onGameLoaded() {
        isGameLoaded = true;
    }

    public static class MemoryPool {
        private final long poolSize;
        private final long blockSize;
        public MemoryPool(long size, long blockSize) {
            if (size <= 0 || blockSize <= 0) {
                throw new IllegalArgumentException("Pool size and block size must be positive");
            }
            this.poolSize = size;
            this.blockSize = blockSize;
            Log.d(TAG, "MemoryPool created with size: " + size + ", block size: " + blockSize);
        }
        public void cleanup() {
            Log.d(TAG, "MemoryPool cleaned up");
        }
    }

    private native boolean nativeInit();
    private native boolean nativeResize(int width, int height);
    private native boolean nativeRenderFrame();
    private native boolean nativeLoadISO(byte[] isoData, int length);
    private native boolean nativeCleanup();
    private native void nativeHandleJoystickInput(float x, float y);
}