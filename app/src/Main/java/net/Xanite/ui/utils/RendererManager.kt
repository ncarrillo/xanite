package com.xanite.utils

import android.content.Context

object RendererManager {
    
    enum class RendererType {
        VULKAN,
        OPENGL
    }
    
    fun getCurrentRenderer(context: Context): RendererType {
        val rendererString = SharedPrefs.getRendererType(context)
        return when (rendererString) {
            "vulkan" -> RendererType.VULKAN
            "opengl" -> RendererType.OPENGL
            else -> RendererType.OPENGL 
        }
    }
    
    fun setRenderer(context: Context, rendererType: RendererType) {
        val rendererString = when (rendererType) {
            RendererType.VULKAN -> "vulkan"
            RendererType.OPENGL -> "opengl"
        }
        SharedPrefs.saveRendererType(context, rendererString)
    }
    
    fun isVulkanEnabled(context: Context): Boolean {
        return getCurrentRenderer(context) == RendererType.VULKAN
    }
    
    fun isOpenGLEnabled(context: Context): Boolean {
        return getCurrentRenderer(context) == RendererType.OPENGL
    }
    
    fun getRendererName(context: Context): String {
        return when (getCurrentRenderer(context)) {
            RendererType.VULKAN -> "Vulkan"
            RendererType.OPENGL -> "OpenGL"
        }
    }
    
    fun getRendererDescription(context: Context): String {
        return when (getCurrentRenderer(context)) {
            RendererType.VULKAN -> "Modern graphics API with better performance"
            RendererType.OPENGL -> "Traditional graphics API with better compatibility"
        }
    }
}
