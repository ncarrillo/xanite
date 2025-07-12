package com.xanite.filemanager

import android.Manifest
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.xanite.databinding.ActivityFileManagerBinding
import com.xanite.filemanager.adapter.FileAdapter
import java.io.File

class FileManagerActivity : AppCompatActivity() {
    private lateinit var binding: ActivityFileManagerBinding
    private val viewModel: FileManagerViewModel by viewModels()
    private lateinit var fileAdapter: FileAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityFileManagerBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupRecyclerView()
        loadAppDirectory()
        setupObservers()
        setupToolbar()
    }

    private fun setupToolbar() {
        binding.toolbar.setNavigationOnClickListener {
            viewModel.getParentDirectory(viewModel.currentDirectory.value ?: "")?.let { parent ->
                viewModel.navigateToDirectory(parent)
            } ?: run {
                Toast.makeText(this, "Already at app root directory", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun setupRecyclerView() {
        fileAdapter = FileAdapter { file ->
            if (file.isDirectory) {
                viewModel.navigateToDirectory(file.absolutePath)
            } else {
                openFile(file)
            }
        }
        
        binding.filesRecyclerView.apply {
            layoutManager = LinearLayoutManager(this@FileManagerActivity)
            adapter = fileAdapter
        }
    }

    private fun openFile(file: File) {
        try {
            val intent = Intent(Intent.ACTION_VIEW)
            val uri = Uri.fromFile(file)
            
            val mimeType = when (file.extension.toLowerCase()) {
                "xml" -> "text/xml"
                "dex" -> "application/octet-stream"
                "arsc" -> "application/octet-stream"
                "bin" -> "application/octet-stream"
                "kt" -> "text/plain"
                "java" -> "text/plain"
                else -> "*/*"
            }

            intent.setDataAndType(uri, mimeType)
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            startActivity(Intent.createChooser(intent, "Open with"))
        } catch (e: Exception) {
            Toast.makeText(this, "Error opening file: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun loadAppDirectory() {
        viewModel.loadFiles("")
    }

    private fun setupObservers() {
        viewModel.fileList.observe(this) { files ->
            fileAdapter.submitList(files)
        }

        viewModel.currentDirectory.observe(this) { directory ->
            binding.currentPathText.text = directory.ifEmpty { "App Internal Files" }
            binding.toolbar.title = File(directory).name.takeIf { it.isNotEmpty() } ?: "App Root"
        }
    }
}