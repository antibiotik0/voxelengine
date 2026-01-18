// =============================================================================
// VOXEL ENGINE - WORLD GENERATOR IMPLEMENTATION
// Superflat generator and factory functions
// =============================================================================

#include "Server/WorldGenerator.hpp"

#include <cstring>

namespace voxel::server {

// =============================================================================
// SUPERFLAT GENERATOR
// =============================================================================

SuperflatGenerator::SuperflatGenerator()
    : m_config(SuperflatConfig::default_config())
{}

SuperflatGenerator::SuperflatGenerator(SuperflatConfig config)
    : m_config(config)
{}

void SuperflatGenerator::generate(Chunk& chunk) {
    const ChunkPosition& pos = chunk.position();

    // Calculate world Y offset for this chunk
    // Chunk Y=0 contains blocks 0-63, Y=1 contains 64-127, etc.
    const ChunkCoord chunk_world_y_base = coord::chunk_to_world(pos.y);

    // Get total terrain height
    const std::uint32_t terrain_height = m_config.total_height();

    // Early exit: chunk is entirely above terrain
    if (chunk_world_y_base >= static_cast<ChunkCoord>(terrain_height)) {
        // Chunk is all air, already zero-initialized
        return;
    }

    // Early exit: chunk is entirely below terrain (if we had void below)
    // For superflat, we only generate down to Y=0
    if (chunk_world_y_base + static_cast<ChunkCoord>(CHUNK_SIZE_Y) <= 0) {
        return;
    }

    // Build a height-to-voxel lookup table for the terrain layers
    // This avoids recalculating layer boundaries for each column
    std::uint16_t layer_types[CHUNK_SIZE_Y];
    std::memset(layer_types, 0, sizeof(layer_types));

    // Calculate which voxel types go at each Y level within this chunk
    for (std::uint32_t local_y = 0; local_y < CHUNK_SIZE_Y; ++local_y) {
        const ChunkCoord world_y = chunk_world_y_base + static_cast<ChunkCoord>(local_y);

        // Skip negative Y (void below bedrock)
        if (world_y < 0) {
            layer_types[local_y] = VoxelType::AIR;
            continue;
        }

        // Skip above terrain height
        if (world_y >= static_cast<ChunkCoord>(terrain_height)) {
            layer_types[local_y] = VoxelType::AIR;
            continue;
        }

        // Find which layer this Y belongs to
        std::uint32_t cumulative_height = 0;
        std::uint16_t block_type = VoxelType::AIR;

        for (std::size_t layer_idx = 0; layer_idx < m_config.layer_count; ++layer_idx) {
            const auto& layer = m_config.layers[layer_idx];
            const std::uint32_t layer_top = cumulative_height + layer.thickness;

            if (static_cast<std::uint32_t>(world_y) < layer_top) {
                block_type = layer.block_type;
                break;
            }
            cumulative_height = layer_top;
        }

        layer_types[local_y] = block_type;
    }

    // Fill the chunk using the precomputed layer types
    // Iterate in column-major order for cache efficiency (Y varies fastest)
    Voxel* data = chunk.data();

    for (LocalCoord x = 0; x < static_cast<LocalCoord>(CHUNK_SIZE_X); ++x) {
        for (LocalCoord z = 0; z < static_cast<LocalCoord>(CHUNK_SIZE_Z); ++z) {
            for (LocalCoord y = 0; y < static_cast<LocalCoord>(CHUNK_SIZE_Y); ++y) {
                const std::uint16_t type = layer_types[y];
                if (type != VoxelType::AIR) {
                    const VoxelIndex idx = coord::to_index(x, y, z);
                    data[idx] = Voxel(type, 0, 0, 0);
                }
            }
        }
    }

    chunk.mark_dirty();
}

bool SuperflatGenerator::should_generate(ChunkPosition pos) const noexcept {
    // Only generate chunks that could contain terrain
    const ChunkCoord chunk_world_y_base = coord::chunk_to_world(pos.y);
    const std::uint32_t terrain_height = m_config.total_height();

    // Don't generate chunks entirely above terrain (optimization)
    if (chunk_world_y_base >= static_cast<ChunkCoord>(terrain_height)) {
        return false;
    }

    // Don't generate chunks below Y=0 (void)
    if (chunk_world_y_base + static_cast<ChunkCoord>(CHUNK_SIZE_Y) <= 0) {
        return false;
    }

    return true;
}

ChunkCoord SuperflatGenerator::get_surface_height(
    [[maybe_unused]] ChunkCoord world_x, 
    [[maybe_unused]] ChunkCoord world_z) const noexcept 
{
    // Superflat has uniform height everywhere
    return static_cast<ChunkCoord>(m_config.total_height());
}

// =============================================================================
// GENERATOR FACTORY
// =============================================================================

namespace generator {

std::unique_ptr<WorldGenerator> create(std::string_view type_name, std::uint64_t seed) {
    if (type_name == "superflat" || type_name == "flat") {
        SuperflatConfig config;
        config.seed = seed;
        return std::make_unique<SuperflatGenerator>(config);
    }

    // Default to superflat
    SuperflatConfig config;
    config.seed = seed;
    return std::make_unique<SuperflatGenerator>(config);
}

} // namespace generator

} // namespace voxel::server
