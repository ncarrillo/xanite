package com.xanite.xboxoriginal

import android.app.ActivityManager
import android.content.Context
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.Rect
import android.os.*
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.getSystemService
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.xanite.databinding.ActivityXboxShaderBinding
import com.xanite.utils.RendererManager
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.system.measureTimeMillis

class XboxShaderActivity : AppCompatActivity(), SurfaceHolder.Callback {
    private lateinit var binding: ActivityXboxShaderBinding
    private var nativePtr: Long = 0
    private val isRunning = AtomicBoolean(false)
    private val isRendering = AtomicBoolean(false)
    private val handler = Handler(Looper.getMainLooper())
    private val executor = Executors.newSingleThreadExecutor()
    private val activityManager by lazy { 
        getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager 
    }

    private val fixedWidth = 1280
    private val fixedHeight = 720
    private var surfaceHolder: SurfaceHolder? = null
    private var destRect: Rect? = null
    
    
    private val byteBuffer = ByteBuffer.allocateDirect(fixedWidth * fixedHeight * 4).apply {
        order(ByteOrder.nativeOrder())
    }
    private val bitmap = Bitmap.createBitmap(fixedWidth, fixedHeight, Bitmap.Config.ARGB_8888)
    private var lastFrameTime = System.currentTimeMillis()
    private var frameCount = 0
    private var isDisplayingGame = false
    private var gamePath: String? = null
    private var isEmulatorInitialized = false
    private var isGameLoaded = false

    companion object {
        private const val TAG = "XboxShaderActivity"
        private const val TARGET_FPS = 60
        private const val FRAME_TIME_MS = 1000 / TARGET_FPS
        
        init {
            try {
                System.loadLibrary("c++_shared")
                System.loadLibrary("xbox_memory")
                System.loadLibrary("x86_core")
                System.loadLibrary("nv2a_renderer")
                System.loadLibrary("xbox_kernel")
                System.loadLibrary("xbox_emulator")
            } catch (e: Exception) {
                Log.e("XboxShader", "Library load error", e)
            }
        }
    
        @JvmStatic external fun nativeIsRunning(ptr: Long): Boolean
        @JvmStatic external fun nativeStopRenderer(ptr: Long)
        @JvmStatic external fun nativeCopyFramebuffer(ptr: Long, byteBuffer: ByteBuffer)
        @JvmStatic external fun nativeSetOutputResolution(ptr: Long, width: Int, height: Int)
        @JvmStatic external fun nativeLoadGame(ptr: Long, gameUri: String, gameType: Int): Boolean
        @JvmStatic external fun nativeCreateInstance(): Long
        @JvmStatic external fun nativeStartEmulation(ptr: Long)
        @JvmStatic external fun nativeShutdown(ptr: Long)
        @JvmStatic external fun nativeCreateNV2ARenderer(ptr: Long): Long
        @JvmStatic external fun nativeCreateVulkanRenderer(ptr: Long): Long
        @JvmStatic external fun nativeCreateOpenGLRenderer(ptr: Long): Long
        @JvmStatic external fun nativeSetOpenGLSurface(ptr: Long, surface: android.view.Surface): Boolean
        @JvmStatic external fun nativeSetPerformanceMode(ptr: Long, enabled: Boolean)
        @JvmStatic external fun nativeInitializeDirect(ptr: Long)
        @JvmStatic external fun nativeStartTestRendering(ptr: Long)
        @JvmStatic external fun nativeLoadTestGame(ptr: Long)
        @JvmStatic external fun nativeInitialize(ptr: Long, biosPath: String, mcpxPath: String, hddPath: String): Boolean
        @JvmStatic external fun nativeIsBiosLoaded(ptr: Long): Boolean
        @JvmStatic external fun nativeLoadISOAndStart(ptr: Long, gamePath: String): Boolean
        @JvmStatic external fun nativeLoadGameFileAndStart(ptr: Long, gamePath: String): Boolean
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(TAG, "XboxShaderActivity created")
        
        binding = ActivityXboxShaderBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        setupFullscreenMode()
        setupSurfaceView()
        showLoading(true)
        
        initializeRenderer()
       
    }

    private fun initializeRenderer() {
        
        val intentRendererType = intent.getStringExtra("renderer_type")
        val rendererType = if (intentRendererType != null) {
            when (intentRendererType) {
                "VULKAN" -> RendererManager.RendererType.VULKAN
                "OPENGL" -> RendererManager.RendererType.OPENGL
                else -> RendererManager.getCurrentRenderer(this)
            }
        } else {
            RendererManager.getCurrentRenderer(this)
        }
        
        Log.i(TAG, "Initializing renderer: $rendererType")
        
        when (rendererType) {
            RendererManager.RendererType.VULKAN -> {
                Log.i(TAG, "Using Vulkan renderer")
            }
            RendererManager.RendererType.OPENGL -> {
                Log.i(TAG, "Using OpenGL renderer")
            }
        }
        
        val gamePath = intent.getStringExtra("game_path")
        if (gamePath != null) {
            Log.i(TAG, "Game path from intent: $gamePath")
            
            this.gamePath = gamePath
        } else {
            
            findAndLoadDefaultGame()
        }
    }

    private fun isLowPerfDevice(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            activityManager.isLowRamDevice || activityManager.memoryClass <= 128
        } else {
            Runtime.getRuntime().availableProcessors() < 4
        }
    }
    
    private fun checkBiosFiles(): Boolean {
        val biosDir = File("/data/user/0/com.xanite/files/bios/")
        if (!biosDir.exists()) {
            Log.w(TAG, "BIOS directory does not exist: ${biosDir.absolutePath}")
            return false
        }
        
        val requiredFiles = listOf(
            "Complex_4627v1.03.bin",
            "mcpx_1.0.bin",
            "xbox_hdd.qcow2"
        )
        
        val missingFiles = mutableListOf<String>()
        val existingFiles = mutableListOf<String>()
        
        for (filename in requiredFiles) {
            val file = File(biosDir, filename)
            if (file.exists()) {
                existingFiles.add("$filename (${file.length()} bytes)")
            } else {
                missingFiles.add(filename)
            }
        }
        
        if (existingFiles.isNotEmpty()) {
            Log.i(TAG, "Found BIOS files: $existingFiles")
        }
        
        if (missingFiles.isNotEmpty()) {
            Log.w(TAG, "Missing BIOS files: $missingFiles")
            return false
        }
        
        Log.i(TAG, "All BIOS files found")
        return true
    }
    
    private fun initializeBiosIfNeeded(): Boolean {
        if (isEmulatorInitialized) {
            Log.i(TAG, "Emulator already initialized")
            return true
        }
        
        Log.i(TAG, "Initializing BIOS...")
        
        if (!checkBiosFiles()) {
            Log.w(TAG, "BIOS files not found, cannot initialize")
            return false
        }
        
        val biosPath = "/data/user/0/com.xanite/files/bios/Complex_4627v1.03.bin"
        val mcpxPath = "/data/user/0/com.xanite/files/bios/mcpx_1.0.bin"
        val hddPath = "/data/user/0/com.xanite/files/bios/xbox_hdd.qcow2"
        
        Log.i(TAG, "Calling nativeInitialize with paths:")
        Log.i(TAG, "  BIOS: $biosPath")
        Log.i(TAG, "  MCPX: $mcpxPath")
        Log.i(TAG, "  HDD: $hddPath")
        
        val initSuccess = nativeInitialize(nativePtr, biosPath, mcpxPath, hddPath)
        Log.i(TAG, "nativeInitialize returned: $initSuccess")
        
        if (initSuccess) {
            Log.i(TAG, "Emulator initialized with BIOS successfully")
            isEmulatorInitialized = true
            
            val biosLoaded = nativeIsBiosLoaded(nativePtr)
            Log.i(TAG, "BIOS loaded verification: $biosLoaded")
            
            if (!biosLoaded) {
                Log.w(TAG, "WARNING: nativeInitialize returned true but BIOS is not loaded!")
                return false
            }
            
            return true
        } else {
            Log.e(TAG, "Failed to initialize with BIOS")
            isEmulatorInitialized = false
            return false
        }
    }
    
       private fun createMinimalXBE(): ByteArray {
        val xbeData = ByteArray(4096) 
        
        xbeData[0] = 'X'.code.toByte()
        xbeData[1] = 'B'.code.toByte()
        xbeData[2] = 'E'.code.toByte()
        xbeData[3] = 'H'.code.toByte()
           
        xbeData[4] = 0x00  
        xbeData[5] = 0x00
        xbeData[6] = 0x00
        xbeData[7] = 0x00
        
        
        xbeData[8] = 0x00
        xbeData[9] = 0x10
        xbeData[10] = 0x00
        xbeData[11] = 0x00
        
        xbeData[12] = 0x00
        xbeData[13] = 0x10
        xbeData[14] = 0x00
        xbeData[15] = 0x00
        
        
        xbeData[16] = 0x00
        xbeData[17] = 0x10
        xbeData[18] = 0x00
        xbeData[19] = 0x00
        
      
        xbeData[20] = 0x00
        xbeData[21] = 0x10
        xbeData[22] = 0x00
        xbeData[23] = 0x00
        
        
        val timestamp = System.currentTimeMillis() / 1000
        xbeData[24] = (timestamp and 0xFF).toByte()
        xbeData[25] = ((timestamp shr 8) and 0xFF).toByte()
        xbeData[26] = ((timestamp shr 16) and 0xFF).toByte()
        xbeData[27] = ((timestamp shr 24) and 0xFF).toByte()
        
        xbeData[28] = 0x00
        xbeData[29] = 0x20
        xbeData[30] = 0x00
        xbeData[31] = 0x00
        
   
        xbeData[32] = 0x01
        xbeData[33] = 0x00
        xbeData[34] = 0x00
        xbeData[35] = 0x00
        
        
        xbeData[36] = 0x00
        xbeData[37] = 0x30
        xbeData[38] = 0x00
        xbeData[39] = 0x00
        
        xbeData[40] = 0x00
        xbeData[41] = 0x00
        xbeData[42] = 0x00
        xbeData[43] = 0x00
        
        xbeData[44] = 0x00
        xbeData[45] = 0x10
        xbeData[46] = 0x00
        xbeData[47] = 0x00
        
        xbeData[48] = 0x00
        xbeData[49] = 0x00
        xbeData[50] = 0x00
        xbeData[51] = 0x00
        
        xbeData[52] = 0x00
        xbeData[53] = 0x10
        xbeData[54] = 0x00
        xbeData[55] = 0x00
        
        xbeData[56] = 0x00
        xbeData[57] = 0x10
        xbeData[58] = 0x00
        xbeData[59] = 0x00
        
        xbeData[60] = 0x00
        xbeData[61] = 0x10
        xbeData[62] = 0x00
        xbeData[63] = 0x00
        
        xbeData[64] = 0x00
        xbeData[65] = 0x40
        xbeData[66] = 0x00
        xbeData[67] = 0x00
        
        xbeData[68] = 0x00
        xbeData[69] = 0x10
        xbeData[70] = 0x00
        xbeData[71] = 0x00
        
        
        xbeData[72] = 0x00
        xbeData[73] = 0x00
        xbeData[74] = 0x00
        xbeData[75] = 0x00
        
        
        xbeData[76] = (timestamp and 0xFF).toByte()
        xbeData[77] = ((timestamp shr 8) and 0xFF).toByte()
        xbeData[78] = ((timestamp shr 16) and 0xFF).toByte()
        xbeData[79] = ((timestamp shr 24) and 0xFF).toByte()
        
        
        xbeData[80] = 0x00
        xbeData[81] = 0x00
        xbeData[82] = 0x00
        xbeData[83] = 0x00
        
        
        xbeData[84] = 0x10
        xbeData[85] = 0x00
        xbeData[86] = 0x00
        xbeData[87] = 0x00
        
        
        for (i in 0 until 16) {
            val offset = 88 + (i * 8)
            xbeData[offset] = 0x00     
            xbeData[offset + 1] = 0x00
            xbeData[offset + 2] = 0x00
            xbeData[offset + 3] = 0x00
            xbeData[offset + 4] = 0x00 
            xbeData[offset + 5] = 0x00
            xbeData[offset + 6] = 0x00
            xbeData[offset + 7] = 0x00
        }
        
        return xbeData
    }
    
    private fun showBiosMissingMessage() {
        handler.post {
            binding.statusText.text = "BIOS files missing - Please extract Xanite_system.zip first"
            Toast.makeText(this@XboxShaderActivity, 
                "Please extract Xanite_system.zip in the main app first", 
                Toast.LENGTH_LONG).show()
        }
    }
    
    private fun findAndLoadDefaultGame() {
        executor.execute {
            try {
              
                val gameDirectories = listOf(
                    "/storage/emulated/0/Download",  
                    "/sdcard/Download",             
                    "/storage/emulated/0/Games",     
                    "/storage/emulated/0/Xbox",      
                    "/storage/emulated/0/ROMs/Xbox", 
                    "/sdcard/Games",               
                    "/sdcard/Xbox",                  
                    "/storage/emulated/0/Android/data/com.xanite.xboxoriginal/files/games"
                )
               
               
                val gameExtensions = listOf(".iso", ".xbe", ".xbox")
                
                var foundGame: String? = null
                
                
                for (directory in gameDirectories) {
                    val dir = File(directory)
                    if (dir.exists() && dir.isDirectory) {
                        Log.d(TAG, "Searching for games in: $directory")
                        
                        for (file in dir.listFiles() ?: emptyArray()) {
                            if (file.isFile && gameExtensions.any { file.name.lowercase().endsWith(it) }) {
                                foundGame = file.absolutePath
                                Log.i(TAG, "Found Xbox game: $foundGame")
                                break
                            }
                        }
                        
                        if (foundGame != null) break
                    }
                }
                
                if (foundGame != null) {
                    gamePath = foundGame
                    Log.i(TAG, "Auto-selected game: $gamePath")
                    
                    
                    handler.post {
                        val fileName = File(foundGame).name
                        binding.statusText.text = "Found game: $fileName"
                        
                        
                        Toast.makeText(this@XboxShaderActivity, 
                            "Found Xbox game: $fileName", 
                            Toast.LENGTH_SHORT).show()
                    }
                } else {
                    Log.w(TAG, "No Xbox games found in common directories")
                    
                    
                    handler.post {
                        binding.statusText.text = "No games found - Place Xbox ISOs in Downloads folder"
                        
                        Toast.makeText(this@XboxShaderActivity, 
                            "Place Xbox ISO files in your Downloads folder", 
                            Toast.LENGTH_LONG).show()
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error searching for games", e)
            }
        }
    }

    private fun initializeEmulatorAsync() {
        executor.execute {
            try {
                
                nativePtr = nativeCreateInstance().takeIf { it != 0L } 
                 ?: run {
                 showErrorAndFinish("Failed to create emulator instance")
                            return@execute
                           }

                  nativeInitializeDirect(nativePtr)
                  
                  
                  if (!initializeBiosIfNeeded()) {
                      Log.w(TAG, "Failed to initialize BIOS, will try again later")
                  }
                
               
                nativeSetOutputResolution(nativePtr, fixedWidth, fixedHeight)
                
                
                if (!isLowPerfDevice()) {
                    nativeSetPerformanceMode(nativePtr, true)
                }
                
                
                val rendererType = RendererManager.getCurrentRenderer(this@XboxShaderActivity)
                val rendererPtr = when (rendererType) {
                    RendererManager.RendererType.VULKAN -> {
                        Log.i(TAG, "Creating Vulkan renderer")
                        nativeCreateVulkanRenderer(nativePtr)
                    }
                    RendererManager.RendererType.OPENGL -> {
                        Log.i(TAG, "Creating OpenGL renderer")
                        nativeCreateOpenGLRenderer(nativePtr)
                    }
                }
                
                if (rendererPtr == 0L) {
                    throw IllegalStateException("Failed to create ${rendererType.name} renderer")
                }
                
                 
                val currentHolder = surfaceHolder
                if (currentHolder != null) {
                    val success = when (rendererType) {
                        RendererManager.RendererType.VULKAN -> {
                            Log.i(TAG, "Setting Vulkan surface")
                            
                            true 
                        }
                        RendererManager.RendererType.OPENGL -> {
                            Log.i(TAG, "Setting OpenGL surface")
                            nativeSetOpenGLSurface(nativePtr, currentHolder.surface)
                        }
                    }
                    
                    if (success) {
                        Log.i(TAG, "${rendererType.name} surface set successfully")
                    } else {
                        Log.e(TAG, "Failed to set ${rendererType.name} surface")
                    }
                }
                
                
                loadAndRunDashboard()
                
                handler.post {
                    showLoading(false)
                    showToast("Xbox Emulator Ready")
                }
                
                nativeStartEmulation(nativePtr)
                isRunning.set(true)
                
                executor.execute(renderRunnable)
                
            } catch (e: Exception) {
                Log.e(TAG, "Emulator initialization failed", e)
                handler.post { 
                    showErrorAndFinish("Initialization failed: ${e.message ?: "Unknown error"}")
                }
            }
        }
    }

    private fun setupFullscreenMode() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, binding.root).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        
        WindowCompat.setDecorFitsSystemWindows(window, false)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode = 
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
    }

    private fun setupSurfaceView() {
        val surfaceView = SurfaceView(this).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            holder.addCallback(this@XboxShaderActivity)
            
           
            holder.setFormat(PixelFormat.RGBA_8888)
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                holder.setFormat(PixelFormat.RGBA_8888)
            }
            
            setZOrderOnTop(true)
            keepScreenOn = true
        }
        
        binding.root.addView(surfaceView, 0)
    }

    private fun showLoading(show: Boolean) {
        runOnUiThread {
            binding.progressBar.visibility = if (show) View.VISIBLE else View.GONE
            binding.statusText.visibility = if (show) View.VISIBLE else View.GONE
        }
    }

    private fun showErrorAndFinish(message: String) {
        runOnUiThread {
            binding.progressBar.visibility = View.GONE
            binding.statusText.text = "Error: $message"
            binding.statusText.visibility = View.VISIBLE
            Handler(Looper.getMainLooper()).postDelayed(::finish, 3000)
        }
    }

    private fun showToast(message: String, duration: Int = Toast.LENGTH_SHORT) {
        runOnUiThread {
            Toast.makeText(this, message, duration).show()
        }
    }

    private fun loadAndRunDashboard() {
        try {
          
            Log.i(TAG, "Lade Rainbow Six 3 Spiel...")
            
            nativeInitializeDirect(nativePtr)
            
            val gamePath = intent.getStringExtra("game_path")
            if (gamePath != null && File(gamePath).exists()) {
                Log.i(TAG, "Lade Spiel von: $gamePath")
                
                if (nativeLoadGameFileAndStart(nativePtr, gamePath)) {
                    Log.i(TAG, "Game erfolgreich geladen und Emulation gestartet!")
                    
                    isGameLoaded = true
                    
                    nativeStartTestRendering(nativePtr)
                    Log.i(TAG, "Test-Rendering gestartet!")
                } else {
                    Log.e(TAG, "Game-Laden fehlgeschlagen")
                    throw IllegalStateException("Game-Laden fehlgeschlagen")
                }
            } else {
                Log.e(TAG, "Kein gültiger Spiel-Pfad bereitgestellt")
                throw IllegalStateException("Keine Spieldatei gefunden")
            }
            
            Log.i(TAG, "Emulator erfolgreich initialisiert")
            
        } catch (e: Exception) {
            Log.e(TAG, "Emulator-Initialisierung fehlgeschlagen", e)
            handler.post {
                showErrorAndFinish("Emulator-Fehler: ${e.message ?: "Unbekannter Fehler"}")
            }
            throw e
        }
    }

    private fun copyAssetToFile(assetName: String, destination: File) {
        try {
            assets.open(assetName).use { inputStream ->
                destination.outputStream().use { outputStream ->
                    inputStream.copyTo(outputStream)
                }
            }
            Log.i(TAG, "Copied $assetName to ${destination.absolutePath}")
        } catch (e: IOException) {
            Log.e(TAG, "Failed to copy asset: $assetName", e)
            throw IOException("Could not copy dashboard from assets", e)
        }
    }

    private fun isValidXbeFile(file: File): Boolean {
        if (!file.exists() || file.length() < 4) return false
        
        return try {
            val magic = ByteArray(4)
            file.inputStream().use { it.read(magic) }
           
            magic[0] == 0x58.toByte() && 
            magic[1] == 0x42.toByte() && 
            magic[2] == 0x45.toByte() && 
            magic[3] == 0x00.toByte()
        } catch (e: Exception) {
            false
        }
    }
    
    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "Surface erstellt - starte Initialisierung")
        surfaceHolder = holder
        lastFrameTime = System.currentTimeMillis()
        
        
        try {
            holder.setFormat(PixelFormat.RGBA_8888)
            Log.d(TAG, "Surface-Format auf RGBA_8888 gesetzt")
        } catch (e: Exception) {
            Log.w(TAG, "Surface-Format-Setzung fehlgeschlagen", e)
        }
        
        val surfaceWidth = holder.surfaceFrame.width()
        val surfaceHeight = holder.surfaceFrame.height()
        if (surfaceWidth > 0 && surfaceHeight > 0) {
            calculateDestRect(surfaceWidth, surfaceHeight)
            Log.d(TAG, "Surface-Dimensionen: ${surfaceWidth}x${surfaceHeight}")
        }
       
        initializeEmulatorAsync()
    }

    private fun calculateDestRect(surfaceWidth: Int, surfaceHeight: Int) {
        val aspectRatio = fixedWidth.toFloat() / fixedHeight
        val surfaceAspect = surfaceWidth.toFloat() / surfaceHeight
        
        val (targetWidth, targetHeight) = if (surfaceAspect > aspectRatio) {
          
            val height = surfaceHeight
            val width = (height * aspectRatio).toInt()
            width to height
        } else {
            
            val width = surfaceWidth
            val height = (width / aspectRatio).toInt()
            width to height
        }
        
        val left = (surfaceWidth - targetWidth) / 2
        val top = (surfaceHeight - targetHeight) / 2
        destRect = Rect(left, top, left + targetWidth, top + targetHeight)
    }
    
    private val renderRunnable = Runnable {
        if (!isRendering.compareAndSet(false, true)) {
            Log.w(TAG, "Rendering already in progress, skipping")
            return@Runnable
        }
        
        try {
            while (isRunning.get() && nativeIsRunning(nativePtr)) {
                try {
                    val currentHolder = surfaceHolder
                    if (currentHolder == null) {
                        Thread.sleep(16)
                        continue
                    }
                    
                    
                    if (currentHolder == null) {
                        Thread.sleep(16)
                        continue
                    }

                try {
                    byteBuffer.rewind()
                    nativeCopyFramebuffer(nativePtr, byteBuffer)
                    bitmap.copyPixelsFromBuffer(byteBuffer)
                   
                    handler.post {
                        try {
                            binding.gameDisplay.setImageBitmap(bitmap)
                            if (!isDisplayingGame) {
                                isDisplayingGame = true
                                Log.i(TAG, "Now displaying real Xbox game graphics!")
                                
                                binding.loadingOverlay.visibility = View.GONE
                                binding.gameDisplay.visibility = View.VISIBLE
                                
                                if (!isGameLoaded) {
                                    if (gamePath != null) {
                                        Log.i(TAG, "Loading real Xbox game: $gamePath")
                                        
                                        if (initializeBiosIfNeeded()) {
                                            
                                            if (isEmulatorInitialized && !isGameLoaded) {
                                                val success = nativeLoadGameFileAndStart(nativePtr, gamePath!!)
                                                if (success) {
                                                    Log.i(TAG, "Real Xbox game loaded successfully!")
                                                    binding.statusText.text = "Playing: ${File(gamePath).name}"
                                                    isGameLoaded = true
                                                } else {
                                                    Log.w(TAG, "Failed to load real game, loading test game")
                                                    nativeLoadTestGame(nativePtr)
                                                    isGameLoaded = true 
                                                }
                                            } else if (isGameLoaded) {
                                                Log.d(TAG, "Game already loaded, skipping reload")
                                            } else {
                                                Log.w(TAG, "Cannot load game - BIOS not initialized")
                                            }
                                        } else {
                                            Log.e(TAG, "Failed to initialize BIOS, cannot load game")
                                        }
                                    } else {
                                        Log.i(TAG, "No game found, loading test game")
                                        nativeLoadTestGame(nativePtr)
                                    }
                                }
                            }
                        } catch (e: Exception) {
                            Log.w(TAG, "Error displaying game graphics: ${e.message}")
                        }
                    }
                    
                    frameCount++
                    if (frameCount % 60 == 0) { 
                        Log.d(TAG, "Real game graphics updated - Frame: $frameCount")
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Error updating game framebuffer: ${e.message}")
                }

              
                val now = System.currentTimeMillis()
                val frameTime = now - lastFrameTime
                lastFrameTime = now
                
                val sleepTime = (FRAME_TIME_MS - frameTime).coerceAtLeast(1).coerceAtMost(32)
                Thread.sleep(sleepTime)
                
            } catch (e: Exception) {
                Log.e(TAG, "Fehler im Rendering-Thread", e)
                Thread.sleep(16)
            }
        }
        Log.d(TAG, "Rendering-Thread gestoppt")
        } finally {
            isRendering.set(false)
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "Surface changed: $width x $height")
        surfaceHolder = holder
        calculateDestRect(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "Surface destroyed - stopping rendering")
        isRunning.set(false)
        surfaceHolder = null
        
        var waitCount = 0
        while (isRendering.get() && waitCount < 10) {
            Thread.sleep(10)
            waitCount++
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
       
        surfaceHolder?.let { holder ->
            val frame = holder.surfaceFrame
            calculateDestRect(frame.width(), frame.height())
        }
    }

    override fun onPause() {
        super.onPause()
        isRunning.set(false)

        var waitCount = 0
        while (isRendering.get() && waitCount < 10) {
            Thread.sleep(10)
            waitCount++
        }
    }

    override fun onResume() {
        super.onResume()
        if (nativePtr != 0L) {
            isRunning.set(true)

            surfaceHolder?.let {
                if (!isRendering.get()) {
                    executor.execute(renderRunnable)
                }
            }
        }
    }

    override fun onDestroy() {
        isRunning.set(false)
        
        if (nativePtr != 0L) {
            try {
  
                nativeShutdown(nativePtr)
                nativePtr = 0L
            } catch (e: Exception) {
                Log.e(TAG, "Error during native shutdown", e)
            }
        }
        
        
        try {
            executor.shutdownNow()
        } catch (e: Exception) {
            Log.e(TAG, "Error during executor shutdown", e)
        }
        
        super.onDestroy()
    }
}