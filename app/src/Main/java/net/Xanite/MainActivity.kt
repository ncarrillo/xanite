package com.xanite

import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.preference.PreferenceManager
import com.xanite.databinding.ActivityMainBinding
import com.xanite.filemanager.FileManagerActivity
import com.xanite.settings.SettingsActivity
import com.xanite.xbox360.Xbox360Activity
import com.xanite.xboxoriginal.XboxOriginalActivity

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private lateinit var sharedPref: SharedPreferences

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        sharedPref = PreferenceManager.getDefaultSharedPreferences(this)
        applyThemeSettings()
        
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        setupToolbar()
        setupClickListeners()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(false)
        supportActionBar?.setDisplayShowTitleEnabled(false)
    }

    private fun setupClickListeners() {
        binding.xboxOriginalCard.setOnClickListener {
            startActivity(Intent(this, XboxOriginalActivity::class.java))
        }

        binding.xbox360Card.setOnClickListener {
            startActivity(Intent(this, Xbox360Activity::class.java))
        }

        binding.systemFilesCard.setOnClickListener {
            startActivity(Intent(this, FileManagerActivity::class.java))
        }
    }

    private fun applyThemeSettings() {
        when (sharedPref.getString("theme_preference", "light")) {
            "light" -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO)
            "dark" -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)
        }
    }

    override fun onCreateOptionsMenu(menu: android.view.Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: android.view.MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_settings -> {
                startActivity(Intent(this, SettingsActivity::class.java))
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onDestroy() {
        super.onDestroy()
    }
}