package com.xanite.xboxoriginal

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.xanite.databinding.ActivityXboxOriginalBinding
import com.xanite.models.Game
import com.xanite.R
import com.xanite.utils.RendererManager
import java.io.File

class XboxOriginalActivity : AppCompatActivity() {
    private lateinit var binding: ActivityXboxOriginalBinding
    private val viewModel: XboxOriginalViewModel by viewModels()
    private lateinit var adapter: XboxOriginalGamesAdapter

    

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityXboxOriginalBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupToolbar()
        setupRecyclerView()
        setupObservers()
        setupFabButton()
        viewModel.loadGames()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            title = getString(R.string.xbox_games) 
        }
    }

    private fun setupFabButton() {
        binding.fabAddGame.setOnClickListener {
            openFilePicker()
        }
    }

    private fun setupRecyclerView() {
        adapter = XboxOriginalGamesAdapter { game ->
            showGameOptionsDialog(game)
        }

       with(binding.recyclerView) {
            layoutManager = GridLayoutManager(this@XboxOriginalActivity, 2)
            this.adapter = this@XboxOriginalActivity.adapter
            addItemDecoration(GridSpacingItemDecoration(2, 16, true))
            setHasFixedSize(true)
        }
    }

    private fun showGameOptionsDialog(game: Game) {
        AlertDialog.Builder(this)
            .setTitle(game.name)
            .setItems(arrayOf("Play", "Delete")) { _, which ->
                when (which) {
                    0 -> playGame(game)
                    1 -> confirmDeleteGame(game)
                }
            }
            .show()
    }

    private fun playGame(game: Game) {
        val rendererType = RendererManager.getCurrentRenderer(this)
        val rendererName = RendererManager.getRendererName(this)
        
        showToast("Launching ${game.name} with $rendererName renderer")
        
        
        val intent = Intent(this, XboxShaderActivity::class.java).apply {
            putExtra("game_path", game.path)
            putExtra("renderer_type", rendererType.name)
        }
        startActivity(intent)
    }

    private fun confirmDeleteGame(game: Game) {
        AlertDialog.Builder(this)
            .setTitle("Delete ${game.name}?")
            .setMessage("Are you sure you want to delete this game?")
            .setPositiveButton("Delete") { _, _ ->
                deleteGame(game)
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun deleteGame(game: Game) {
        val file = File(game.path)
        if (file.exists()) {
            val deleted = file.delete()
            if (deleted) {
                showToast("Game deleted")
                viewModel.loadGames()
            } else {
                showToast("Failed to delete game")
            }
        } else {
            showToast("Game file not found")
        }
    }

    private fun setupObservers() {
        viewModel.gamesList.observe(this) { games ->
            adapter.submitList(games)
            if (games.isEmpty()) {
                showToast("No games found. Please add a new game.")
            }
        }

        viewModel.errorMessage.observe(this) { error ->
            error?.let { showToast(it) }
        }

        viewModel.successMessage.observe(this) { message ->
            message?.let { showToast(it) }
        }

        viewModel.isLoading.observe(this) { loading ->
            binding.progressBar.visibility = if (loading) View.VISIBLE else View.GONE
            binding.fabAddGame.isEnabled = !loading
        }
    }

    
    private fun showToast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }
 

  private fun openFilePicker() {
    val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
        addCategory(Intent.CATEGORY_OPENABLE)
        type = "*/*"
        putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true) 
    }
    filePickerLauncher.launch(intent)
}

private val filePickerLauncher = registerForActivityResult(
    ActivityResultContracts.StartActivityForResult()
) { result ->
    if (result.resultCode == Activity.RESULT_OK) {
        result.data?.let { intent ->
            if (intent.clipData != null) {

                for (i in 0 until intent.clipData!!.itemCount) {
                    val uri = intent.clipData!!.getItemAt(i).uri
                    viewModel.addGame(uri)
                }
            } else {
                
                intent.data?.let { uri ->
                    viewModel.addGame(uri)
                }
            }
        }
    }
}

class GridSpacingItemDecoration(
    private val spanCount: Int,
    private val spacing: Int,
    private val includeEdge: Boolean
) : RecyclerView.ItemDecoration() {
    override fun getItemOffsets(
        outRect: android.graphics.Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        val position = parent.getChildAdapterPosition(view)
        val column = position % spanCount

        if (includeEdge) {
            outRect.left = spacing - column * spacing / spanCount
            outRect.right = (column + 1) * spacing / spanCount
            if (position < spanCount) {
                outRect.top = spacing
            }
            outRect.bottom = spacing
        } else {
            outRect.left = column * spacing / spanCount
            outRect.right = spacing - (column + 1) * spacing / spanCount
            if (position >= spanCount) {
                outRect.top = spacing
            }
        }
    }
}
}