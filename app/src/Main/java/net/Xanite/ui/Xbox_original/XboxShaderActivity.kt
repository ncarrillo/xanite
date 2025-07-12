package com.xanite.xboxoriginal

import android.content.Intent
import android.os.Bundle
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.xanite.databinding.ActivityXboxShaderBinding
import com.xanite.overlay.PadOverlay
import android.view.ViewGroup

class XboxShaderActivity : AppCompatActivity() {
    private lateinit var binding: ActivityXboxShaderBinding
    private lateinit var padOverlay: PadOverlay

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        enableFullScreenImmersive()
        
        binding = ActivityXboxShaderBinding.inflate(layoutInflater)
        setContentView(binding.root)

        initializePadOverlay()

        handleGamePath(intent)
    }

    private fun initializePadOverlay() {
        padOverlay = PadOverlay(this, null).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            isEditing = false // تعطيل وضع التحرير
        }
        binding.root.addView(padOverlay)
    }

    private fun handleGamePath(intent: Intent) {
        val gamePath = intent.getStringExtra("GAME_PATH") ?: ""
        // here Don't forget C++
        
    }

    private fun enableFullScreenImmersive() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        window.setFlags(
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS
        )
        
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    override fun onDestroy() {
        super.onDestroy()
        binding.root.removeView(padOverlay)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enableFullScreenImmersive()
    }
}