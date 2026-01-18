// =============================================================================
// VOXEL ENGINE - WORLD MANAGEMENT
// Thread-safe chunk container with concurrent read access
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"

#include <memory>
#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <vector>

namespace voxel::server {

// Forward declarations
class WorldGenerator;

// =============================================================================
// WORLD CONFIGURATION
// =============================================================================
struct WorldConfig {
    // World seed for procedural generation
    std::uint64_t seed = 0;

    // Vertical chunk range (in chunk coordinates)
    ChunkCoord min_chunk_y = -4;   // -256 blocks
    ChunkCoord max_chunk_y = 16;   // +1024 blocks

    // World name for serialization
    const char* name = "world";

    // Generator type identifier
    const char* generator_type = "superflat";
};

// =============================================================================
// WORLD CLASS
// Manages chunk storage with thread-safe concurrent access
// Uses reader-writer lock pattern for optimal read performance
// =============================================================================
class World {
public:
    // Chunk storage type - unique_ptr for exclusive ownership
    using ChunkPtr = std::unique_ptr<Chunk>;
    using ChunkMap = std::unordered_map<ChunkPosition, ChunkPtr>;

    // Callback types
    using ChunkCallback = std::function<void(Chunk&)>;
    using ChunkConstCallback = std::function<void(const Chunk&)>;

    // =============================================================================
    // CONSTRUCTION
    // =============================================================================

    World();
    explicit World(WorldConfig config);
    ~World();

    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept;
    World& operator=(World&&) noexcept;

    // =============================================================================
    // CONFIGURATION
    // =============================================================================

    [[nodiscard]] const WorldConfig& config() const noexcept { return m_config; }
    [[nodiscard]] std::uint64_t seed() const noexcept { return m_config.seed; }

    void set_generator(std::unique_ptr<WorldGenerator> generator);
    [[nodiscard]] WorldGenerator* generator() noexcept { return m_generator.get(); }
    [[nodiscard]] const WorldGenerator* generator() const noexcept { return m_generator.get(); }

    // =============================================================================
    // CHUNK ACCESS (Thread-Safe)
    // =============================================================================

    // Get chunk at position (read-only, shared lock)
    [[nodiscard]] const Chunk* get_chunk(ChunkPosition pos) const;
    [[nodiscard]] const Chunk* get_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z) const;

    // Get chunk for modification (exclusive lock)
    [[nodiscard]] Chunk* get_chunk_mut(ChunkPosition pos);
    [[nodiscard]] Chunk* get_chunk_mut(ChunkCoord x, ChunkCoord y, ChunkCoord z);

    // Check if chunk exists (shared lock)
    [[nodiscard]] bool has_chunk(ChunkPosition pos) const;
    [[nodiscard]] bool has_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z) const;

    // =============================================================================
    // CHUNK LIFECYCLE
    // =============================================================================

    // Load or generate chunk at position
    // Returns pointer to the chunk (never null after successful call)
    Chunk* load_chunk(ChunkPosition pos);
    Chunk* load_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z);

    // Unload chunk at position (saves if dirty)
    bool unload_chunk(ChunkPosition pos);
    bool unload_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z);

    // Insert pre-created chunk (takes ownership)
    bool insert_chunk(ChunkPosition pos, ChunkPtr chunk);

    // Remove chunk without saving
    ChunkPtr remove_chunk(ChunkPosition pos);

    // =============================================================================
    // VOXEL ACCESS (Thread-Safe, Cross-Chunk)
    // =============================================================================

    // Get voxel at world coordinates
    [[nodiscard]] Voxel get_voxel(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z) const;

    // Set voxel at world coordinates (loads chunk if needed)
    bool set_voxel(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z, Voxel voxel);

    // Safe voxel access with bounds checking
    [[nodiscard]] std::optional<Voxel> get_voxel_safe(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z) const;

    // =============================================================================
    // BLOCK MANIPULATION (Phase 3)
    // =============================================================================

    // Break block at world coordinates
    // Returns the voxel that was broken (or air if nothing was there)
    // Marks affected chunk(s) as dirty
    Voxel break_block(std::int64_t world_x, std::int64_t world_y, std::int64_t world_z);

    // Place block at world coordinates
    // Returns true if placement was successful
    // Marks affected chunk(s) as dirty
    bool place_block(std::int64_t world_x, std::int64_t world_y, std::int64_t world_z, Voxel voxel);

    // =============================================================================
    // DIRTY CHUNK TRACKING
    // =============================================================================

    // Check if any chunks need mesh rebuild
    [[nodiscard]] bool has_dirty_chunks() const;

    // Get list of dirty chunk positions and clear the dirty set
    [[nodiscard]] std::vector<ChunkPosition> consume_dirty_chunks();

    // Mark a chunk as dirty (for external use)
    void mark_chunk_dirty(ChunkPosition pos);

    // =============================================================================
    // BULK OPERATIONS
    // =============================================================================

    // Get count of loaded chunks
    [[nodiscard]] std::size_t chunk_count() const;

    // Iterate over all chunks (shared lock held during iteration)
    void for_each_chunk(ChunkConstCallback callback) const;

    // Iterate with mutable access (exclusive lock)
    void for_each_chunk_mut(ChunkCallback callback);

    // Get all loaded chunk positions
    [[nodiscard]] std::vector<ChunkPosition> get_loaded_positions() const;

    // Unload all chunks
    void unload_all();

    // =============================================================================
    // COORDINATE UTILITIES
    // =============================================================================

    // Convert world coordinates to chunk position
    [[nodiscard]] static ChunkPosition world_to_chunk_pos(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z) noexcept;

    // Convert world coordinates to local chunk coordinates
    [[nodiscard]] static LocalCoord world_to_local(ChunkCoord world) noexcept;

    // Check if chunk Y is within valid range
    [[nodiscard]] bool is_valid_chunk_y(ChunkCoord chunk_y) const noexcept;

    // Check if world coordinates are within bounds
    [[nodiscard]] static bool is_valid_world_pos(ChunkCoord x, ChunkCoord z) noexcept;

private:
    // Generate chunk data using the world generator
    void generate_chunk(Chunk& chunk);

    // Internal chunk lookup (caller must hold appropriate lock)
    [[nodiscard]] const Chunk* find_chunk_unlocked(ChunkPosition pos) const;
    [[nodiscard]] Chunk* find_chunk_unlocked(ChunkPosition pos);

private:
    WorldConfig m_config;
    std::unique_ptr<WorldGenerator> m_generator;

    // Chunk storage with reader-writer lock
    mutable std::shared_mutex m_chunks_mutex;
    ChunkMap m_chunks;

    // Dirty chunks that need mesh rebuild
    mutable std::mutex m_dirty_mutex;
    std::unordered_set<ChunkPosition> m_dirty_chunks;

    // Statistics
    std::uint64_t m_chunks_generated = 0;
    std::uint64_t m_chunks_loaded = 0;
    std::uint64_t m_chunks_unloaded = 0;
};

} // namespace voxel::server
