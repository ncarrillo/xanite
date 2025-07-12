package com.xanite.filemanager

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import java.io.File

class FileManagerViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = FileManagerRepository(application)
    
    val fileList = MutableLiveData<List<File>>()
    val currentDirectory = MutableLiveData<String>()

    fun loadFiles(directory: String) {
        currentDirectory.value = directory
        fileList.value = repository.getFiles(directory)
    }

    fun navigateToDirectory(path: String) {
        loadFiles(path)
    }

    fun getParentDirectory(currentPath: String): String? {
        val parent = File(currentPath).parent
        return if (parent != null && File(parent).exists()) parent else null
    }
}