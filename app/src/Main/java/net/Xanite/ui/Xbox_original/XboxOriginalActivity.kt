package com.xanite.xboxoriginal

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.Observer
import androidx.recyclerview.widget.LinearLayoutManager
import com.xanite.databinding.ActivityXboxOriginalBinding
import com.xanite.models.Game

class XboxOriginalActivity : AppCompatActivity() {
    private lateinit var binding: ActivityXboxOriginalBinding
    private val viewModel: XboxOriginalViewModel by viewModels()
    private lateinit var gamesAdapter: XboxOriginalGamesAdapter
    private var selectedGame: Game? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityXboxOriginalBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupUI()
        setupObservers()
        loadData()
    }

    private fun setupUI() {
        gamesAdapter = XboxOriginalGamesAdapter { game ->
            selectedGame = game
            launchGame(game.path)
        }

        binding.gameRecyclerView.apply {
            layoutManager = LinearLayoutManager(this@XboxOriginalActivity)
            adapter = gamesAdapter
        }

        binding.fabAddGame.setOnClickListener {
            openFilePicker()
        }
    }

    private fun setupObservers() {
        viewModel.biosList.observe(this, Observer { biosFiles ->
            biosFiles?.let { files ->
                binding.biosInfoTextView.text = if (files.isNotEmpty()) {
                    "BIOS Files: ${files.joinToString()}"
                } else {
                    "No BIOS files found. Copy BIOS files to ${filesDir}/bios/"
                }
            }
        })

        viewModel.gamesList.observe(this, Observer { games ->
            games?.let {
                gamesAdapter.submitList(it)
                binding.emptyState.visibility = if (it.isEmpty()) View.VISIBLE else View.GONE
                if (it.isNotEmpty()) selectedGame = it[0]
            }
        })

        viewModel.errorMessage.observe(this, Observer { error ->
            Toast.makeText(this, error ?: "Unknown error", Toast.LENGTH_LONG).show()
        })

        viewModel.successMessage.observe(this, Observer { message ->
            Toast.makeText(this, message ?: "Success", Toast.LENGTH_SHORT).show()
        })
    }

    private fun loadData() {
        viewModel.loadBios()
        viewModel.loadGames()
    }

    private fun openFilePicker() {
        Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/x-iso9660-image"))
        }.also { startActivityForResult(it, PICK_GAME_FILE) }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == PICK_GAME_FILE && resultCode == RESULT_OK) {
            data?.data?.let { uri ->
                viewModel.addGame(uri)
            }
        }
    }

    private fun launchGame(gamePath: String) {
        try {
            val intent = Intent(this, XboxShaderActivity::class.java).apply {
                putExtra("GAME_PATH", gamePath)
                addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)
            }
            startActivity(intent)
        } catch (e: Exception) {
            Toast.makeText(this, "Error: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    companion object {
        private const val PICK_GAME_FILE = 1001
    }
}