package com.xanite

import android.Manifest
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.net.Uri
import android.os.*
import android.provider.Settings
import android.util.Log
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.preference.PreferenceManager
import com.xanite.databinding.ActivityMainBinding
import com.xanite.databinding.SimpleDownloadDialogBinding
import com.xanite.filemanager.FileManagerActivity
import com.xanite.settings.SettingsActivity
import com.xanite.xbox360.Xbox360Activity
import com.xanite.xboxoriginal.XboxOriginalActivity
import java.io.*
import java.util.Locale
import java.util.zip.ZipInputStream

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var sharedPref: SharedPreferences
    private val STORAGE_PERMISSION_REQUEST_CODE = 1
    private val SETTINGS_REQUEST_CODE = 1001
    private val BIOS_DIR_PATH = "/data/user/0/com.xanite/files/bios/"
    private val FIRST_RUN_KEY = "first_run"
    private val ZIP_FILENAME = "Xanite_system.zip"
    private val REQUIRED_BIOS_FILES = listOf(
        "Complex_4627v1.03.bin",
        "mcpx_1.0.bin",
        "xbox_hdd.qcow2"
    )

   
    private val filePickerLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == RESULT_OK) {
            result.data?.data?.let { uri ->
                showExtractProgress(uri)
            }
        } else {
            Toast.makeText(this, getString(R.string.file_selection_cancelled), Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        sharedPref = PreferenceManager.getDefaultSharedPreferences(this)
        applyAppSettings()

        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupToolbar()
        setupClickListeners()
        checkAndRequestPermissions()

        
        if (isFirstRun() && !areBiosFilesPresent()) {
            showFilePickerDialog()
        }
    }

    private fun isFirstRun(): Boolean {
        val firstRun = sharedPref.getBoolean(FIRST_RUN_KEY, true)
        if (firstRun) {
            sharedPref.edit().putBoolean(FIRST_RUN_KEY, false).apply()
        }
        return firstRun
    }

    private fun areBiosFilesPresent(): Boolean {
        val biosDir = File(BIOS_DIR_PATH)
        if (!biosDir.exists()) {
            return false
        }
        return REQUIRED_BIOS_FILES.all { filename -> 
            val file = File(biosDir, filename)
            file.exists() && file.length() > 0
        }
    }

    private fun showFilePickerDialog() {
    val dialogBinding = SimpleDownloadDialogBinding.inflate(layoutInflater)
    val dialog = AlertDialog.Builder(this)
        .setView(dialogBinding.root)
        .setCancelable(true)
        .create()

    with(dialogBinding) {
        titleText.text = getString(R.string.select_bios_file)
        
        
        messageText.text = getString(R.string.select_bios_message)
        messageText.visibility = View.VISIBLE
        
        nextButton.text = getString(R.string.select_file)
        closeButton.text = getString(R.string.cancel)
        
        nextButton.setOnClickListener {
            dialog.dismiss()
            openFilePicker()
        }
        
        closeButton.setOnClickListener {
            dialog.dismiss()
            Toast.makeText(this@MainActivity, getString(R.string.select_file_later), Toast.LENGTH_SHORT).show()
        }
    }

    dialog.show()
}

    private fun openFilePicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/zip"
            putExtra(Intent.EXTRA_TITLE, "Select $ZIP_FILENAME")
            flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or 
                    Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
        }
        filePickerLauncher.launch(intent)
    }

    private fun showExtractProgress(uri: Uri) {
        val progressDialogBinding = SimpleDownloadDialogBinding.inflate(layoutInflater)
        val progressDialog = AlertDialog.Builder(this)
            .setView(progressDialogBinding.root)
            .setCancelable(false)
            .create()

        with(progressDialogBinding) {
            titleText.text = getString(R.string.extracting_files)
            messageText.text = getString(R.string.extracting_message)
            messageText.visibility = View.VISIBLE
            nextButton.visibility = View.GONE
            closeButton.visibility = View.GONE
            progressBar.visibility = View.VISIBLE
            progressText.visibility = View.VISIBLE
            progressBar.max = 100
            progressBar.progress = 0
        }

        progressDialog.show()

        extractZipFile(uri, progressDialogBinding) {
            progressDialog.dismiss()
            if (areBiosFilesPresent()) {
                Toast.makeText(this, getString(R.string.extraction_complete), Toast.LENGTH_SHORT).show()
            } else {
                showExtractionFailedDialog()
            }
        }
    }

    private fun showExtractionFailedDialog() {
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.extraction_failed))
            .setMessage(getString(R.string.extraction_failed_message))
            .setPositiveButton(getString(R.string.retry)) { _, _ ->
                showFilePickerDialog()
            }
            .setNegativeButton(getString(R.string.cancel), null)
            .show()
    }

    private fun extractZipFile(uri: Uri, dialogBinding: SimpleDownloadDialogBinding, onComplete: () -> Unit) {
        Thread {
            try {
                val biosDir = File(BIOS_DIR_PATH)
                if (!biosDir.exists()) {
                    biosDir.mkdirs()
                }

                contentResolver.takePersistableUriPermission(
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                )

                contentResolver.openInputStream(uri)?.use { inputStream ->
                    ZipInputStream(BufferedInputStream(inputStream)).use { zipInputStream ->
                        var entry = zipInputStream.nextEntry
                        var extractedEntries = 0
                        val totalEntries = REQUIRED_BIOS_FILES.size

                        while (entry != null) {
                            if (!entry.isDirectory && REQUIRED_BIOS_FILES.contains(entry.name)) {
                                val outputFile = File(biosDir, entry.name)
                                FileOutputStream(outputFile).use { outputStream ->
                                    val buffer = ByteArray(1024 * 8)
                                    var length: Int
                                    var totalRead: Long = 0
                                    val entrySize = entry.size

                                    while (zipInputStream.read(buffer).also { length = it } > 0) {
                                        outputStream.write(buffer, 0, length)
                                        totalRead += length.toLong()

                                        
                                        val entryProgress = if (entrySize > 0) {
                                            (totalRead * 100 / entrySize).toInt()
                                        } else {
                                            0
                                        }
                                        val overallProgress = ((extractedEntries * 100) + entryProgress) / totalEntries
                                        
                                        runOnUiThread {
                                            dialogBinding.progressBar.progress = overallProgress
                                            dialogBinding.progressText.text = "$overallProgress%"
                                        }
                                    }
                                }
                                extractedEntries++
                                Log.d("Extraction", "Extracted: ${entry.name}")
                            }
                            zipInputStream.closeEntry()
                            entry = zipInputStream.nextEntry
                        }
                    }
                }

                runOnUiThread(onComplete)

            } catch (e: Exception) {
                e.printStackTrace()
                runOnUiThread {
                    Toast.makeText(this, "${getString(R.string.extraction_failed)}: ${e.message}", Toast.LENGTH_LONG).show()
                }
            }
        }.start()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(false)
        supportActionBar?.setDisplayShowTitleEnabled(false)
    }

    private fun setupClickListeners() {
        binding.apply {
            xboxOriginalCard.setOnClickListener {
                startActivity(Intent(this@MainActivity, XboxOriginalActivity::class.java))
            }

            xbox360Card.setOnClickListener {
                startActivity(Intent(this@MainActivity, Xbox360Activity::class.java))
            }

            systemFilesCard.setOnClickListener {
                startActivity(Intent(this@MainActivity, FileManagerActivity::class.java))
            }

            biosDownloadCard.setOnClickListener {
              
                showFilePickerDialog()
            }
        }
    }

    private fun applyAppSettings() {
        applyThemeSettings()
        applyLanguageSettings()
    }

    private fun applyThemeSettings() {
        when (sharedPref.getString("theme_preference", "light")) {
            "light" -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO)
            "dark" -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)
            else -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
        }
    }

    private fun applyLanguageSettings() {
        val languageCode = sharedPref.getString("language_preference", "en") ?: "en"
        val locale = Locale(languageCode)
        Locale.setDefault(locale)
        val config = Configuration()
        config.setLocale(locale)
        baseContext.resources.updateConfiguration(config, baseContext.resources.displayMetrics)
    }

    private fun checkAndRequestPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                showStoragePermissionDialog()
            }
        } else {
            val readPermission = Manifest.permission.READ_EXTERNAL_STORAGE
            if (ContextCompat.checkSelfPermission(this, readPermission) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, arrayOf(readPermission), STORAGE_PERMISSION_REQUEST_CODE)
            }
        }
    }

    private fun showStoragePermissionDialog() {
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.storage_permission_needed))
            .setMessage(getString(R.string.storage_permission_message))
            .setPositiveButton(getString(R.string.grant)) { _, _ ->
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    startActivity(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
                } else {
                    ActivityCompat.requestPermissions(
                        this,
                        arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE),
                        STORAGE_PERMISSION_REQUEST_CODE
                    )
                }
            }
            .setNegativeButton(getString(R.string.cancel)) { dialog, _ -> dialog.dismiss() }
            .show()
    }

    override fun onCreateOptionsMenu(menu: android.view.Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: android.view.MenuItem): Boolean {
        if (item.itemId == R.id.action_settings) {
            startActivityForResult(Intent(this, SettingsActivity::class.java), SETTINGS_REQUEST_CODE)
            return true
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == SETTINGS_REQUEST_CODE) {
            recreate()
        }
    }

    override fun onResume() {
        super.onResume()
        checkForSettingsChanges()
    }

    private fun checkForSettingsChanges() {
        val currentTheme = sharedPref.getString("theme_preference", "light")
        val shouldBeDark = currentTheme == "dark"
        val isDarkMode = resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES
        
        val currentLanguage = resources.configuration.locale.language
        val savedLanguage = sharedPref.getString("language_preference", "en") ?: "en"
        
        if (isDarkMode != shouldBeDark || currentLanguage != savedLanguage) {
            recreate()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == STORAGE_PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
                Toast.makeText(this, getString(R.string.permission_granted), Toast.LENGTH_SHORT).show()
                showFilePickerDialog()
            } else {
                Toast.makeText(this, getString(R.string.permission_denied), Toast.LENGTH_SHORT).show()
            }
        }
    }
}