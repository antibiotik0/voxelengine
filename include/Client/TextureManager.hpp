// =============================================================================
// VOXEL ENGINE - TEXTURE MANAGER
// GL_TEXTURE_2D_ARRAY based texture atlas for efficient block rendering
// Supports mipmap generation and GL_REPEAT for greedy meshing
// =============================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <filesystem>

// stb_image for PNG loading
#include <stb_image.h>

// glad for OpenGL functions
#include <glad/glad.h>

namespace voxel::client {

// =============================================================================
// TEXTURE MANAGER
// Loads all .png files from a directory into a GL_TEXTURE_2D_ARRAY
// =============================================================================
class TextureManager {
public:
    static constexpr std::uint32_t TEXTURE_SIZE = 16;      // Each texture is 16x16
    static constexpr std::uint32_t MAX_LAYERS = 256;       // Max texture layers
    
    TextureManager() = default;
    ~TextureManager() { destroy(); }
    
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    // ==========================================================================
    // INITIALIZATION
    // ==========================================================================
    
    // Load all .png files from directory into texture array
    bool load_from_directory(const std::string& directory_path) {
        namespace fs = std::filesystem;
        
        if (!fs::exists(directory_path)) {
            std::printf("[TextureManager] Directory not found: %s\n", directory_path.c_str());
            return false;
        }
        
        // Collect all PNG files
        std::vector<std::string> png_files;
        for (const auto& entry : fs::directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png") {
                    png_files.push_back(entry.path().string());
                }
            }
        }
        
        if (png_files.empty()) {
            std::printf("[TextureManager] No PNG files found in: %s\n", directory_path.c_str());
            return create_default_texture();
        }
        
        // Sort for consistent ordering
        std::sort(png_files.begin(), png_files.end());
        
        std::printf("[TextureManager] Found %zu textures in %s\n", 
                    png_files.size(), directory_path.c_str());
        
        return create_texture_array(png_files);
    }
    
    // ==========================================================================
    // TEXTURE LOOKUP
    // ==========================================================================
    
    // Get layer index by filename (returns -1 if not found)
    [[nodiscard]] std::int32_t get_layer(std::string_view filename) const {
        std::string key(filename);
        auto it = m_name_to_layer.find(key);
        if (it != m_name_to_layer.end()) {
            return static_cast<std::int32_t>(it->second);
        }
        return -1;
    }
    
    // Get total layer count
    [[nodiscard]] std::uint32_t layer_count() const noexcept { 
        return m_layer_count; 
    }
    
    // Get OpenGL texture ID
    [[nodiscard]] std::uint32_t texture_id() const noexcept { 
        return m_texture_array; 
    }
    
    // ==========================================================================
    // BINDING
    // ==========================================================================
    
    void bind(std::uint32_t unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture_array);
    }
    
    // ==========================================================================
    // DEBUG
    // ==========================================================================
    
    void list_textures() const {
        std::printf("[TextureManager] Loaded textures:\n");
        for (const auto& [name, layer] : m_name_to_layer) {
            std::printf("  Layer %u: %s\n", layer, name.c_str());
        }
    }

private:
    std::uint32_t m_texture_array = 0;
    std::uint32_t m_layer_count = 0;
    std::unordered_map<std::string, std::uint32_t> m_name_to_layer;
    
    void destroy() {
        if (m_texture_array != 0) {
            glDeleteTextures(1, &m_texture_array);
            m_texture_array = 0;
        }
        m_layer_count = 0;
        m_name_to_layer.clear();
    }
    
    bool create_texture_array(const std::vector<std::string>& png_files) {
        namespace fs = std::filesystem;
        
        std::uint32_t num_layers = static_cast<std::uint32_t>(
            std::min(png_files.size(), static_cast<std::size_t>(MAX_LAYERS))
        );
        
        // Calculate mipmap levels (log2 of texture size)
        std::uint32_t mip_levels = 1;
        std::uint32_t size = TEXTURE_SIZE;
        while (size > 1) {
            size >>= 1;
            mip_levels++;
        }
        
        // Create texture array using DSA
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_texture_array);
        
        // Allocate storage for all layers with mipmaps
        glTextureStorage3D(m_texture_array, 
                           static_cast<GLsizei>(mip_levels),
                           GL_RGBA8, 
                           static_cast<GLsizei>(TEXTURE_SIZE), 
                           static_cast<GLsizei>(TEXTURE_SIZE), 
                           static_cast<GLsizei>(num_layers));
        
        // Load each texture into a layer
        std::vector<std::uint8_t> default_pixels(TEXTURE_SIZE * TEXTURE_SIZE * 4, 255);
        
        for (std::uint32_t layer = 0; layer < num_layers; ++layer) {
            const std::string& filepath = png_files[layer];
            std::string filename = fs::path(filepath).filename().string();
            
            int width, height, channels;
            unsigned char* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
            
            if (pixels && width == static_cast<int>(TEXTURE_SIZE) && 
                height == static_cast<int>(TEXTURE_SIZE)) {
                glTextureSubImage3D(m_texture_array, 0,
                                   0, 0, static_cast<GLint>(layer),
                                   static_cast<GLsizei>(TEXTURE_SIZE), 
                                   static_cast<GLsizei>(TEXTURE_SIZE), 1,
                                   GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                
                m_name_to_layer[filename] = layer;
                std::printf("[TextureManager] Loaded: %s -> layer %u\n", filename.c_str(), layer);
            } else {
                // Use default white if load failed or wrong size
                glTextureSubImage3D(m_texture_array, 0,
                                   0, 0, static_cast<GLint>(layer),
                                   static_cast<GLsizei>(TEXTURE_SIZE), 
                                   static_cast<GLsizei>(TEXTURE_SIZE), 1,
                                   GL_RGBA, GL_UNSIGNED_BYTE, default_pixels.data());
                
                if (pixels) {
                    std::printf("[TextureManager] Wrong size for %s (%dx%d, expected %ux%u)\n",
                               filename.c_str(), width, height, TEXTURE_SIZE, TEXTURE_SIZE);
                } else {
                    std::printf("[TextureManager] Failed to load: %s\n", filename.c_str());
                }
                m_name_to_layer[filename] = layer;
            }
            
            if (pixels) {
                stbi_image_free(pixels);
            }
        }
        
        m_layer_count = num_layers;
        
        // Generate mipmaps
        glGenerateTextureMipmap(m_texture_array);
        
        // =======================================================================
        // CRITICAL: Set GL_REPEAT for greedy meshing UV stretching
        // =======================================================================
        glTextureParameteri(m_texture_array, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_texture_array, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // Use NEAREST for crisp pixels, but LINEAR mipmap for distance
        glTextureParameteri(m_texture_array, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_texture_array, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        
        // Anisotropic filtering for better quality at angles
        glTextureParameterf(m_texture_array, GL_TEXTURE_MAX_ANISOTROPY, 16.0f);
        
        std::printf("[TextureManager] Created texture array: %u layers, %u mip levels\n", 
                    m_layer_count, mip_levels);
        std::printf("[TextureManager] GL_REPEAT enabled for greedy mesh UV tiling\n");
        
        return true;
    }
    
    bool create_default_texture() {
        // Create a single default magenta/black checkerboard texture
        std::vector<std::uint8_t> pixels(TEXTURE_SIZE * TEXTURE_SIZE * 4);
        for (std::uint32_t y = 0; y < TEXTURE_SIZE; ++y) {
            for (std::uint32_t x = 0; x < TEXTURE_SIZE; ++x) {
                std::size_t idx = (y * TEXTURE_SIZE + x) * 4;
                bool checker = ((x / 4) + (y / 4)) % 2 == 0;
                pixels[idx + 0] = checker ? 255 : 0;    // R
                pixels[idx + 1] = 0;                     // G
                pixels[idx + 2] = checker ? 255 : 0;    // B
                pixels[idx + 3] = 255;                   // A
            }
        }
        
        // Create using DSA
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_texture_array);
        glTextureStorage3D(m_texture_array, 4, GL_RGBA8, 
                           static_cast<GLsizei>(TEXTURE_SIZE), 
                           static_cast<GLsizei>(TEXTURE_SIZE), 1);
        glTextureSubImage3D(m_texture_array, 0, 0, 0, 0, 
                            static_cast<GLsizei>(TEXTURE_SIZE), 
                            static_cast<GLsizei>(TEXTURE_SIZE), 1,
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateTextureMipmap(m_texture_array);
        
        // GL_REPEAT for greedy meshing
        glTextureParameteri(m_texture_array, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_texture_array, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(m_texture_array, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_texture_array, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        
        m_layer_count = 1;
        m_name_to_layer["default.png"] = 0;
        
        std::printf("[TextureManager] Created default checkerboard texture\n");
        return true;
    }
};

} // namespace voxel::client
