package com.xanite.filemanager.adapter

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.xanite.R
import java.io.File
import java.text.SimpleDateFormat
import java.util.*

class FileAdapter(private val onItemClick: (File) -> Unit) : 
    ListAdapter<File, FileAdapter.FileViewHolder>(FileDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): FileViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_file, parent, false)
        return FileViewHolder(view, parent.context)
    }

    override fun onBindViewHolder(holder: FileViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class FileViewHolder(itemView: View, private val context: Context) : RecyclerView.ViewHolder(itemView) {
        private val icon: ImageView = itemView.findViewById(R.id.fileIcon)
        private val name: TextView = itemView.findViewById(R.id.fileName)
        private val info: TextView = itemView.findViewById(R.id.fileInfo)

        fun bind(file: File) {
            name.text = file.name
            
            icon.setImageResource(
                when {
                    file.isDirectory -> R.drawable.ic_folder
                    file.extension.equals("xml", ignoreCase = true) -> R.drawable.code
                    file.extension.equals("dex", ignoreCase = true) -> R.drawable.code
                    file.extension.equals("arsc", ignoreCase = true) -> R.drawable.code
                    file.extension.equals("bin", ignoreCase = true) -> R.drawable.code
                    file.extension.equals("kt", ignoreCase = true) -> R.drawable.code
                    file.extension.equals("java", ignoreCase = true) -> R.drawable.code
                    else -> R.drawable.ic_file
                }
            )

            info.text = try {
                if (file.isDirectory) {
                    "${file.list()?.size ?: 0} items"
                } else {
                    "${formatFileSize(file.length())} • ${formatDate(file.lastModified())}"
                }
            } catch (e: Exception) {
                "Unknown"
            }

            itemView.setOnClickListener { onItemClick(file) }
        }

        private fun formatFileSize(size: Long): String {
            return when {
                size > 1024 * 1024 * 1024 -> "%.2f GB".format(size / (1024.0 * 1024.0 * 1024.0))
                size > 1024 * 1024 -> "%.2f MB".format(size / (1024.0 * 1024.0))
                size > 1024 -> "%.2f KB".format(size / 1024.0)
                else -> "$size B"
            }
        }

        private fun formatDate(timestamp: Long): String {
            return SimpleDateFormat("MM/dd/yyyy HH:mm", Locale.US)
                .format(Date(timestamp))
        }
    }
}

class FileDiffCallback : DiffUtil.ItemCallback<File>() {
    override fun areItemsTheSame(oldItem: File, newItem: File): Boolean {
        return oldItem.absolutePath == newItem.absolutePath
    }

    override fun areContentsTheSame(oldItem: File, newItem: File): Boolean {
        return oldItem.lastModified() == newItem.lastModified() && 
               oldItem.length() == newItem.length()
    }
}