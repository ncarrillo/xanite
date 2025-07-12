package com.xanite.xboxoriginal

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.xanite.models.Game
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class XboxOriginalViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = XboxOriginalRepository(application)
    
    val biosList = MutableLiveData<List<String>>()
    val gamesList = MutableLiveData<List<Game>>()
    val errorMessage = MutableLiveData<String>()
    val successMessage = MutableLiveData<String>()

    fun loadGames() {
        viewModelScope.launch {
            try {
                val games = withContext(Dispatchers.IO) {
                    repository.getGames()
                }
                gamesList.postValue(games)
            } catch (e: Exception) {
                errorMessage.postValue("Failed to load games: ${e.message}")
            }
        }
    }

    fun loadBios() {
        viewModelScope.launch {
            try {
                val biosFiles = withContext(Dispatchers.IO) {
                    repository.getBiosFiles()
                }
                biosList.postValue(biosFiles)
            } catch (e: Exception) {
                errorMessage.postValue("Failed to load BIOS files: ${e.message}")
            }
        }
    }

    fun addGame(uri: Uri) {
        viewModelScope.launch {
            try {
                val result = withContext(Dispatchers.IO) {
                    repository.addGame(uri)
                }
                if (result) {
                    loadGames()
                    successMessage.postValue("Game added successfully")
                } else {
                    errorMessage.postValue("Failed to add game")
                }
            } catch (e: Exception) {
                errorMessage.postValue("Error: ${e.message}")
            }
        }
    }

    fun copyBiosFromAssets(biosName: String) {
        viewModelScope.launch {
            try {
                val result = withContext(Dispatchers.IO) {
                    repository.copyBiosFromAssets(biosName)
                }
                if (result) {
                    loadBios()
                    successMessage.postValue("BIOS copied successfully")
                } else {
                    errorMessage.postValue("Failed to copy BIOS")
                }
            } catch (e: Exception) {
                errorMessage.postValue("Error: ${e.message}")
            }
        }
    }
}