// =============================================================================
// VOXEL ENGINE - BLOCK REGISTRY
// Data-Driven Block Properties - Single Source of Truth
// Loads block definitions from config/blocks.toml
// =============================================================================
#pragma once

#include "Types.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>

namespace voxel {

// =============================================================================
// BLOCK PROPERTIES (POD, cache-friendly)
// =============================================================================
struct BlockProperties {
    char name[32] = "unknown";          // Block name (fixed size, no allocation)
    std::uint16_t id = 0;               // Block type ID
    
    // Visual properties
    std::uint8_t texture_top = 0;       // Texture index for +Y face
    std::uint8_t texture_side = 0;      // Texture index for ±X, ±Z faces
    std::uint8_t texture_bottom = 0;    // Texture index for -Y face
    
    // Physical properties
    bool is_solid = true;               // Collision enabled
    bool is_transparent = false;        // Light passes through
    bool is_fluid = false;              // Fluid simulation applies
    
    // Fluid properties (only if is_fluid = true)
    std::uint8_t fluid_viscosity = 2;   // Ticks between spread (higher = slower)
    std::uint8_t fluid_max_distance = 7;// Maximum horizontal spread distance
    std::uint16_t fluid_source_id = 0;  // Source block type for this fluid
    
    // Light properties
    std::uint8_t light_emission = 0;    // 0-15 light level emitted
    std::uint8_t light_filter = 15;     // How much light is blocked (15 = opaque)
    
    // Rendering flags
    bool render_all_faces = false;      // Don't cull faces (e.g., glass, leaves)
    
    // Helper methods
    [[nodiscard]] constexpr bool blocks_light() const noexcept {
        return light_filter > 0 && !is_transparent;
    }
    
    [[nodiscard]] constexpr bool has_collision() const noexcept {
        return is_solid && !is_fluid;
    }
};

static_assert(sizeof(BlockProperties) <= 64, "BlockProperties should fit in a cache line");

// =============================================================================
// BLOCK REGISTRY (Singleton)
// =============================================================================
class BlockRegistry {
public:
    static constexpr std::size_t MAX_BLOCK_TYPES = 256;  // First 256 reserved
    
    // Singleton access
    static BlockRegistry& instance() {
        static BlockRegistry registry;
        return registry;
    }
    
    // Load block definitions from file
    bool load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::printf("[BlockRegistry] Failed to open: %s\n", filepath.c_str());
            register_defaults();
            return false;
        }
        
        std::string line;
        BlockProperties current_block;
        bool in_block = false;
        std::uint16_t blocks_loaded = 0;
        
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);
            
            // Section header [[blocks.name]]
            if (line.size() > 4 && line[0] == '[' && line[1] == '[') {
                // Save previous block if valid
                if (in_block && current_block.id < MAX_BLOCK_TYPES) {
                    m_blocks[current_block.id] = current_block;
                    blocks_loaded++;
                }
                
                // Parse block name from [[blocks.name]]
                size_t dot = line.find('.');
                size_t end = line.find("]]");
                if (dot != std::string::npos && end != std::string::npos) {
                    std::string name = line.substr(dot + 1, end - dot - 1);
                    current_block = BlockProperties{};
                    std::strncpy(current_block.name, name.c_str(), 31);
                    current_block.name[31] = '\0';
                    in_block = true;
                }
                continue;
            }
            
            // Key = value pairs
            if (!in_block) continue;
            
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;
            
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim
            key.erase(key.find_last_not_of(" \t") + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            
            parse_property(current_block, key, value);
        }
        
        // Save last block
        if (in_block && current_block.id < MAX_BLOCK_TYPES) {
            m_blocks[current_block.id] = current_block;
            blocks_loaded++;
        }
        
        std::printf("[BlockRegistry] Loaded %u block types from %s\n", 
                    blocks_loaded, filepath.c_str());
        
        // Ensure defaults are set for any missing blocks
        ensure_defaults();
        
        return true;
    }
    
    // Get block properties by ID
    [[nodiscard]] const BlockProperties& get(std::uint16_t id) const noexcept {
        if (id < MAX_BLOCK_TYPES) {
            return m_blocks[id];
        }
        return m_blocks[0]; // Return air for invalid IDs
    }
    
    // Convenience accessors
    [[nodiscard]] bool is_solid(std::uint16_t id) const noexcept {
        return get(id).is_solid;
    }
    
    [[nodiscard]] bool is_transparent(std::uint16_t id) const noexcept {
        return get(id).is_transparent;
    }
    
    [[nodiscard]] bool is_fluid(std::uint16_t id) const noexcept {
        return get(id).is_fluid;
    }
    
    [[nodiscard]] bool has_collision(std::uint16_t id) const noexcept {
        return get(id).has_collision();
    }
    
    [[nodiscard]] std::string_view name(std::uint16_t id) const noexcept {
        return get(id).name;
    }
    
    // Get all fluid block IDs (for fluid simulation)
    [[nodiscard]] const std::array<std::uint16_t, 16>& fluid_types() const noexcept {
        return m_fluid_ids;
    }
    
    [[nodiscard]] std::size_t fluid_count() const noexcept {
        return m_fluid_count;
    }
    
private:
    BlockRegistry() {
        register_defaults();
    }
    
    void register_defaults() {
        // Air (ID 0)
        m_blocks[VoxelType::AIR] = BlockProperties{
            "air", VoxelType::AIR, 0, 0, 0,
            false, true, false, 0, 0, 0, 0, 0, false
        };
        
        // Stone (ID 1)
        m_blocks[VoxelType::STONE] = BlockProperties{
            "stone", VoxelType::STONE, 1, 1, 1,
            true, false, false, 0, 0, 0, 0, 15, false
        };
        
        // Dirt (ID 2)
        m_blocks[VoxelType::DIRT] = BlockProperties{
            "dirt", VoxelType::DIRT, 2, 2, 2,
            true, false, false, 0, 0, 0, 0, 15, false
        };
        
        // Grass (ID 3)
        m_blocks[VoxelType::GRASS] = BlockProperties{
            "grass", VoxelType::GRASS, 3, 4, 2,
            true, false, false, 0, 0, 0, 0, 15, false
        };
        
        // Water (ID 4)
        m_blocks[VoxelType::WATER] = BlockProperties{
            "water", VoxelType::WATER, 5, 5, 5,
            false, true, true, 4, 7, VoxelType::WATER, 0, 2, true
        };
        
        // Sand (ID 5)
        m_blocks[VoxelType::SAND] = BlockProperties{
            "sand", VoxelType::SAND, 6, 6, 6,
            true, false, false, 0, 0, 0, 0, 15, false
        };
        
        // Wood (ID 6)
        m_blocks[VoxelType::WOOD] = BlockProperties{
            "wood", VoxelType::WOOD, 7, 8, 7,
            true, false, false, 0, 0, 0, 0, 15, false
        };
        
        // Leaves (ID 7)
        m_blocks[VoxelType::LEAVES] = BlockProperties{
            "leaves", VoxelType::LEAVES, 9, 9, 9,
            true, true, false, 0, 0, 0, 0, 1, true
        };
        
        // Glass (ID 8)
        m_blocks[VoxelType::GLASS] = BlockProperties{
            "glass", VoxelType::GLASS, 10, 10, 10,
            true, true, false, 0, 0, 0, 0, 0, true
        };
        
        // Light (ID 9)
        m_blocks[VoxelType::LIGHT] = BlockProperties{
            "light", VoxelType::LIGHT, 11, 11, 11,
            true, false, false, 0, 0, 0, 15, 15, false
        };
        
        update_fluid_list();
    }
    
    void ensure_defaults() {
        // Ensure Air is always defined correctly
        if (m_blocks[0].id != 0) {
            m_blocks[0] = BlockProperties{
                "air", 0, 0, 0, 0, false, true, false, 0, 0, 0, 0, 0, false
            };
        }
        update_fluid_list();
    }
    
    void update_fluid_list() {
        m_fluid_count = 0;
        for (std::size_t i = 0; i < MAX_BLOCK_TYPES && m_fluid_count < 16; ++i) {
            if (m_blocks[i].is_fluid) {
                m_fluid_ids[m_fluid_count++] = static_cast<std::uint16_t>(i);
            }
        }
    }
    
    void parse_property(BlockProperties& block, const std::string& key, const std::string& value) {
        if (key == "id") {
            block.id = static_cast<std::uint16_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "is_solid") {
            block.is_solid = (value == "true" || value == "1");
        } else if (key == "is_transparent") {
            block.is_transparent = (value == "true" || value == "1");
        } else if (key == "is_fluid") {
            block.is_fluid = (value == "true" || value == "1");
        } else if (key == "fluid_viscosity") {
            block.fluid_viscosity = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "fluid_max_distance") {
            block.fluid_max_distance = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "light_emission") {
            block.light_emission = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "light_filter") {
            block.light_filter = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "texture_top") {
            block.texture_top = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "texture_side") {
            block.texture_side = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "texture_bottom") {
            block.texture_bottom = static_cast<std::uint8_t>(std::strtol(value.c_str(), nullptr, 10));
        } else if (key == "render_all_faces") {
            block.render_all_faces = (value == "true" || value == "1");
        }
    }
    
    std::array<BlockProperties, MAX_BLOCK_TYPES> m_blocks{};
    std::array<std::uint16_t, 16> m_fluid_ids{};
    std::size_t m_fluid_count = 0;
};

} // namespace voxel
