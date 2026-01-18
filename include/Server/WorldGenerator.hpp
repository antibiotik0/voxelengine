// =============================================================================
// VOXEL ENGINE - WORLD GENERATOR INTERFACE
// Abstract base for procedural and preset world generation
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace voxel::server {

// =============================================================================
// GENERATOR CONFIGURATION BASE
// =============================================================================
struct GeneratorConfig {
    std::uint64_t seed = 0;

    virtual ~GeneratorConfig() = default;
};

// =============================================================================
// WORLD GENERATOR INTERFACE
// All generators must implement this interface
// =============================================================================
class WorldGenerator {
public:
    virtual ~WorldGenerator() = default;

    // =============================================================================
    // CORE INTERFACE
    // =============================================================================

    // Generate voxel data for the given chunk
    // The chunk is pre-allocated and zero-initialized (air)
    // Implementation should populate the chunk's voxel data
    virtual void generate(Chunk& chunk) = 0;

    // Get generator type identifier
    [[nodiscard]] virtual std::string_view type_name() const noexcept = 0;

    // Get the seed used for generation
    [[nodiscard]] virtual std::uint64_t seed() const noexcept = 0;

    // =============================================================================
    // OPTIONAL OVERRIDES
    // =============================================================================

    // Called once before generation starts (for initialization)
    virtual void initialize() {}

    // Check if chunk at position should be generated
    // Return false to skip generation (e.g., for void chunks)
    [[nodiscard]] virtual bool should_generate([[maybe_unused]] ChunkPosition pos) const noexcept {
        return true;
    }

    // Get the "natural" surface height at world X/Z coordinates
    // Used for spawn point calculation and structure placement
    [[nodiscard]] virtual ChunkCoord get_surface_height(
        [[maybe_unused]] ChunkCoord world_x, 
        [[maybe_unused]] ChunkCoord world_z) const noexcept {
        return 64; // Default surface height
    }
};

// =============================================================================
// SUPERFLAT GENERATOR CONFIGURATION
// =============================================================================
struct SuperflatConfig : public GeneratorConfig {
    // Layer definition: from bottom to top
    struct Layer {
        std::uint16_t block_type;   // Voxel type ID
        std::uint32_t thickness;    // Number of blocks in this layer
    };

    // Default layers: bedrock(1) + stone(3) + dirt(3) + grass(1) = 8 blocks
    static constexpr std::size_t MAX_LAYERS = 16;
    Layer layers[MAX_LAYERS] = {
        { VoxelType::STONE, 1 },    // Bedrock substitute (y=0)
        { VoxelType::STONE, 3 },    // Stone (y=1-3)
        { VoxelType::DIRT, 3 },     // Dirt (y=4-6)
        { VoxelType::GRASS, 1 },    // Grass (y=7)
        { 0, 0 },                   // Terminator
    };
    std::size_t layer_count = 4;

    // Total height of the terrain
    [[nodiscard]] std::uint32_t total_height() const noexcept {
        std::uint32_t height = 0;
        for (std::size_t i = 0; i < layer_count && i < MAX_LAYERS; ++i) {
            height += layers[i].thickness;
        }
        return height;
    }

    // Create default superflat config
    static SuperflatConfig default_config() {
        return SuperflatConfig{};
    }

    // Create flat stone world
    static SuperflatConfig stone_world(std::uint32_t height = 64) {
        SuperflatConfig config;
        config.layers[0] = { VoxelType::STONE, height };
        config.layer_count = 1;
        return config;
    }

    // Create classic superflat (bedrock, dirt, grass)
    static SuperflatConfig classic() {
        SuperflatConfig config;
        config.layers[0] = { VoxelType::STONE, 1 };  // Bedrock
        config.layers[1] = { VoxelType::DIRT, 2 };   // Dirt
        config.layers[2] = { VoxelType::GRASS, 1 };  // Grass
        config.layer_count = 3;
        return config;
    }
};

// =============================================================================
// SUPERFLAT GENERATOR
// Generates flat terrain based on configurable layers
// =============================================================================
class SuperflatGenerator final : public WorldGenerator {
public:
    SuperflatGenerator();
    explicit SuperflatGenerator(SuperflatConfig config);
    ~SuperflatGenerator() override = default;

    // =============================================================================
    // WorldGenerator Interface
    // =============================================================================

    void generate(Chunk& chunk) override;

    [[nodiscard]] std::string_view type_name() const noexcept override {
        return "superflat";
    }

    [[nodiscard]] std::uint64_t seed() const noexcept override {
        return m_config.seed;
    }

    [[nodiscard]] bool should_generate(ChunkPosition pos) const noexcept override;

    [[nodiscard]] ChunkCoord get_surface_height(
        ChunkCoord world_x, 
        ChunkCoord world_z) const noexcept override;

    // =============================================================================
    // Configuration
    // =============================================================================

    [[nodiscard]] const SuperflatConfig& config() const noexcept { return m_config; }
    void set_config(SuperflatConfig config) noexcept { m_config = config; }

private:
    SuperflatConfig m_config;
};

// =============================================================================
// GENERATOR FACTORY
// =============================================================================
namespace generator {

    // Create generator by type name
    [[nodiscard]] std::unique_ptr<WorldGenerator> create(
        std::string_view type_name, 
        std::uint64_t seed = 0);

    // Create superflat generator with default config
    [[nodiscard]] inline std::unique_ptr<SuperflatGenerator> create_superflat() {
        return std::make_unique<SuperflatGenerator>();
    }

    // Create superflat generator with custom config
    [[nodiscard]] inline std::unique_ptr<SuperflatGenerator> create_superflat(SuperflatConfig config) {
        return std::make_unique<SuperflatGenerator>(std::move(config));
    }

} // namespace generator

} // namespace voxel::server
