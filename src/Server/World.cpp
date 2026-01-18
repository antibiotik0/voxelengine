// =============================================================================
// VOXEL ENGINE - WORLD IMPLEMENTATION
// Thread-safe chunk management with concurrent read access
// =============================================================================

#include "Server/World.hpp"
#include "Server/WorldGenerator.hpp"

#include <algorithm>
#include <mutex>

namespace voxel::server {

// =============================================================================
// CONSTRUCTION / DESTRUCTION
// =============================================================================

World::World()
    : m_config{}
    , m_generator(nullptr)
    , m_chunks{}
{}

World::World(WorldConfig config)
    : m_config(config)
    , m_generator(nullptr)
    , m_chunks{}
{}

World::~World() {
    unload_all();
}

World::World(World&& other) noexcept
    : m_config(other.m_config)
    , m_generator(std::move(other.m_generator))
    , m_chunks(std::move(other.m_chunks))
    , m_chunks_generated(other.m_chunks_generated)
    , m_chunks_loaded(other.m_chunks_loaded)
    , m_chunks_unloaded(other.m_chunks_unloaded)
{}

World& World::operator=(World&& other) noexcept {
    if (this != &other) {
        unload_all();
        m_config = other.m_config;
        m_generator = std::move(other.m_generator);
        m_chunks = std::move(other.m_chunks);
        m_chunks_generated = other.m_chunks_generated;
        m_chunks_loaded = other.m_chunks_loaded;
        m_chunks_unloaded = other.m_chunks_unloaded;
    }
    return *this;
}

void World::set_generator(std::unique_ptr<WorldGenerator> generator) {
    m_generator = std::move(generator);
    if (m_generator) {
        m_generator->initialize();
    }
}

// =============================================================================
// CHUNK ACCESS (Thread-Safe)
// =============================================================================

const Chunk* World::get_chunk(ChunkPosition pos) const {
    std::shared_lock lock(m_chunks_mutex);
    return find_chunk_unlocked(pos);
}

const Chunk* World::get_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z) const {
    return get_chunk(ChunkPosition{x, y, z});
}

Chunk* World::get_chunk_mut(ChunkPosition pos) {
    std::unique_lock lock(m_chunks_mutex);
    return find_chunk_unlocked(pos);
}

Chunk* World::get_chunk_mut(ChunkCoord x, ChunkCoord y, ChunkCoord z) {
    return get_chunk_mut(ChunkPosition{x, y, z});
}

bool World::has_chunk(ChunkPosition pos) const {
    std::shared_lock lock(m_chunks_mutex);
    return m_chunks.find(pos) != m_chunks.end();
}

bool World::has_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z) const {
    return has_chunk(ChunkPosition{x, y, z});
}

// =============================================================================
// CHUNK LIFECYCLE
// =============================================================================

Chunk* World::load_chunk(ChunkPosition pos) {
    // Check vertical bounds
    if (!is_valid_chunk_y(pos.y)) {
        return nullptr;
    }

    // Check horizontal bounds
    if (!is_valid_world_pos(coord::chunk_to_world(pos.x), coord::chunk_to_world(pos.z))) {
        return nullptr;
    }

    // First, check if chunk already exists (shared lock)
    {
        std::shared_lock lock(m_chunks_mutex);
        auto* existing = find_chunk_unlocked(pos);
        if (existing) {
            return existing;
        }
    }

    // Chunk doesn't exist, need to create it (exclusive lock)
    std::unique_lock lock(m_chunks_mutex);

    // Double-check after acquiring exclusive lock
    auto* existing = find_chunk_unlocked(pos);
    if (existing) {
        return existing;
    }

    // Create new chunk
    auto chunk = std::make_unique<Chunk>(pos);

    // Generate chunk data
    generate_chunk(*chunk);

    // Insert into map
    Chunk* result = chunk.get();
    m_chunks.emplace(pos, std::move(chunk));
    ++m_chunks_loaded;

    return result;
}

Chunk* World::load_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z) {
    return load_chunk(ChunkPosition{x, y, z});
}

bool World::unload_chunk(ChunkPosition pos) {
    std::unique_lock lock(m_chunks_mutex);

    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) {
        return false;
    }

    // TODO: Save chunk if dirty (Phase 3: Serialization)
    // if (it->second->is_dirty()) {
    //     save_chunk(*it->second);
    // }

    m_chunks.erase(it);
    ++m_chunks_unloaded;
    return true;
}

bool World::unload_chunk(ChunkCoord x, ChunkCoord y, ChunkCoord z) {
    return unload_chunk(ChunkPosition{x, y, z});
}

bool World::insert_chunk(ChunkPosition pos, ChunkPtr chunk) {
    if (!chunk) {
        return false;
    }

    std::unique_lock lock(m_chunks_mutex);

    // Check if position already occupied
    if (m_chunks.find(pos) != m_chunks.end()) {
        return false;
    }

    chunk->set_position(pos);
    m_chunks.emplace(pos, std::move(chunk));
    return true;
}

World::ChunkPtr World::remove_chunk(ChunkPosition pos) {
    std::unique_lock lock(m_chunks_mutex);

    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) {
        return nullptr;
    }

    ChunkPtr chunk = std::move(it->second);
    m_chunks.erase(it);
    return chunk;
}

// =============================================================================
// VOXEL ACCESS (Cross-Chunk)
// =============================================================================

Voxel World::get_voxel(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z) const {
    // Convert world to chunk position using bit-shifts
    ChunkPosition chunk_pos = world_to_chunk_pos(world_x, world_y, world_z);

    // Get chunk (shared lock acquired inside)
    const Chunk* chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return Voxel{}; // Air for non-existent chunks
    }

    // Convert to local coordinates using bit masking
    LocalCoord local_x = world_to_local(world_x);
    LocalCoord local_y = world_to_local(world_y);
    LocalCoord local_z = world_to_local(world_z);

    return chunk->get(local_x, local_y, local_z);
}

bool World::set_voxel(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z, Voxel voxel) {
    // Convert world to chunk position using bit-shifts
    ChunkPosition chunk_pos = world_to_chunk_pos(world_x, world_y, world_z);

    // Load chunk if needed (will acquire locks internally)
    Chunk* chunk = load_chunk(chunk_pos);
    if (!chunk) {
        return false; // Out of bounds
    }

    // Now get exclusive access to modify
    std::unique_lock lock(m_chunks_mutex);

    // Re-find chunk under lock
    Chunk* locked_chunk = find_chunk_unlocked(chunk_pos);
    if (!locked_chunk) {
        return false;
    }

    // Convert to local coordinates using bit masking
    LocalCoord local_x = world_to_local(world_x);
    LocalCoord local_y = world_to_local(world_y);
    LocalCoord local_z = world_to_local(world_z);

    locked_chunk->set(local_x, local_y, local_z, voxel);
    return true;
}

std::optional<Voxel> World::get_voxel_safe(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z) const {
    // Check horizontal bounds
    if (!is_valid_world_pos(world_x, world_z)) {
        return std::nullopt;
    }

    // Check vertical bounds (convert to chunk Y)
    ChunkCoord chunk_y = coord::world_to_chunk(world_y);
    if (!is_valid_chunk_y(chunk_y)) {
        return std::nullopt;
    }

    return get_voxel(world_x, world_y, world_z);
}

// =============================================================================
// BULK OPERATIONS
// =============================================================================

std::size_t World::chunk_count() const {
    std::shared_lock lock(m_chunks_mutex);
    return m_chunks.size();
}

void World::for_each_chunk(ChunkConstCallback callback) const {
    if (!callback) return;

    std::shared_lock lock(m_chunks_mutex);
    for (const auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            callback(*chunk);
        }
    }
}

void World::for_each_chunk_mut(ChunkCallback callback) {
    if (!callback) return;

    std::unique_lock lock(m_chunks_mutex);
    for (auto& [pos, chunk] : m_chunks) {
        if (chunk) {
            callback(*chunk);
        }
    }
}

std::vector<ChunkPosition> World::get_loaded_positions() const {
    std::shared_lock lock(m_chunks_mutex);

    std::vector<ChunkPosition> positions;
    positions.reserve(m_chunks.size());

    for (const auto& [pos, chunk] : m_chunks) {
        positions.push_back(pos);
    }

    return positions;
}

void World::unload_all() {
    std::unique_lock lock(m_chunks_mutex);

    // TODO: Save dirty chunks (Phase 3)

    m_chunks_unloaded += m_chunks.size();
    m_chunks.clear();
}

// =============================================================================
// COORDINATE UTILITIES
// =============================================================================

ChunkPosition World::world_to_chunk_pos(ChunkCoord world_x, ChunkCoord world_y, ChunkCoord world_z) noexcept {
    return ChunkPosition{
        coord::world_to_chunk(world_x),
        coord::world_to_chunk(world_y),
        coord::world_to_chunk(world_z)
    };
}

LocalCoord World::world_to_local(ChunkCoord world) noexcept {
    return coord::world_to_local(world);
}

bool World::is_valid_chunk_y(ChunkCoord chunk_y) const noexcept {
    return chunk_y >= m_config.min_chunk_y && chunk_y <= m_config.max_chunk_y;
}

bool World::is_valid_world_pos(ChunkCoord x, ChunkCoord z) noexcept {
    return x >= WORLD_BOUND_MIN && x <= WORLD_BOUND_MAX &&
           z >= WORLD_BOUND_MIN && z <= WORLD_BOUND_MAX;
}

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

void World::generate_chunk(Chunk& chunk) {
    if (m_generator && m_generator->should_generate(chunk.position())) {
        m_generator->generate(chunk);
        ++m_chunks_generated;
    }
    chunk.set_state(Chunk::State::LOADED);
}

const Chunk* World::find_chunk_unlocked(ChunkPosition pos) const {
    auto it = m_chunks.find(pos);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

Chunk* World::find_chunk_unlocked(ChunkPosition pos) {
    auto it = m_chunks.find(pos);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

} // namespace voxel::server
