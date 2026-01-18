// =============================================================================
// VOXEL ENGINE - CHUNK CONTAINER
// Sacred Interface: 64-byte aligned, 64^3 voxel storage
// Cache-optimized memory layout for SIMD/AVX-512 operations
// =============================================================================
#pragma once

#include "Types.hpp"

#include <memory>
#include <cstring>
#include <new>

namespace voxel {

// =============================================================================
// CHUNK: Primary voxel storage container
// - 64-byte alignment for cache line optimization
// - Flat 1D array for maximum cache locality
// - Column-major ordering (Y varies fastest) for vertical access patterns
// =============================================================================
class alignas(64) Chunk {
public:
    // =============================================================================
    // CONSTANTS
    // =============================================================================
    static constexpr std::uint32_t SIZE_X = CHUNK_SIZE_X;
    static constexpr std::uint32_t SIZE_Y = CHUNK_SIZE_Y;
    static constexpr std::uint32_t SIZE_Z = CHUNK_SIZE_Z;
    static constexpr std::uint32_t VOLUME = CHUNK_VOLUME; // 64^3 = 262,144

    // Memory footprint: 262,144 voxels * 4 bytes = 1,048,576 bytes (1 MiB)
    static constexpr std::size_t DATA_SIZE_BYTES = VOLUME * sizeof(Voxel);

    // =============================================================================
    // CHUNK STATE FLAGS
    // =============================================================================
    enum class State : std::uint8_t {
        UNLOADED    = 0,    // No data loaded
        LOADING     = 1,    // Currently loading from disk/generating
        LOADED      = 2,    // Data ready, not modified
        DIRTY       = 3,    // Modified, needs mesh rebuild
        MESHING     = 4,    // Currently generating mesh
        READY       = 5,    // Mesh ready for rendering
        UNLOADING   = 6     // Scheduled for unload
    };

private:
    // Voxel data - 64-byte aligned unique_ptr with custom deleter
    struct AlignedDeleter {
        void operator()(Voxel* ptr) const noexcept {
            if (ptr) {
                ::operator delete[](ptr, std::align_val_t{64});
            }
        }
    };
    
    std::unique_ptr<Voxel[], AlignedDeleter> m_voxels;

    // Chunk world position
    ChunkPosition m_position;

    // State tracking
    State m_state;

    // Dirty region tracking (optimization for partial mesh updates)
    bool m_fully_dirty;

public:
    // =============================================================================
    // CONSTRUCTION / DESTRUCTION
    // =============================================================================
    
    // Default constructor - creates empty (air-filled) chunk
    Chunk() 
        : m_voxels(nullptr)
        , m_position{}
        , m_state(State::UNLOADED)
        , m_fully_dirty(false) 
    {}

    // Construct with position and allocate memory
    explicit Chunk(ChunkPosition pos)
        : m_voxels(allocate_voxels())
        , m_position(pos)
        , m_state(State::LOADED)
        , m_fully_dirty(true)
    {
        // Zero-initialize all voxels (air) - use raw uint32_t pointer to avoid class-memaccess warning
        std::memset(static_cast<void*>(m_voxels.get()), 0, DATA_SIZE_BYTES);
    }

    // Move-only semantics (no copying 1 MiB chunks)
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) noexcept = default;
    Chunk& operator=(Chunk&&) noexcept = default;

    ~Chunk() = default;

    // =============================================================================
    // VOXEL ACCESS (Hot Path - Inlined)
    // =============================================================================

    // Get voxel at local coordinates (no bounds checking - hot path)
    [[nodiscard]] Voxel get(LocalCoord x, LocalCoord y, LocalCoord z) const noexcept {
        return m_voxels[coord::to_index(x, y, z)];
    }

    // Get voxel by flat index (no bounds checking - hot path)
    [[nodiscard]] Voxel get(VoxelIndex index) const noexcept {
        return m_voxels[index];
    }

    // Set voxel at local coordinates (no bounds checking - hot path)
    void set(LocalCoord x, LocalCoord y, LocalCoord z, Voxel voxel) noexcept {
        m_voxels[coord::to_index(x, y, z)] = voxel;
        mark_dirty();
    }

    // Set voxel by flat index (no bounds checking - hot path)
    void set(VoxelIndex index, Voxel voxel) noexcept {
        m_voxels[index] = voxel;
        mark_dirty();
    }

    // Safe access with bounds checking
    [[nodiscard]] Voxel get_safe(LocalCoord x, LocalCoord y, LocalCoord z) const noexcept {
        if (!coord::is_valid_local(x, y, z)) {
            return Voxel{}; // Air for out-of-bounds
        }
        return get(x, y, z);
    }

    bool set_safe(LocalCoord x, LocalCoord y, LocalCoord z, Voxel voxel) noexcept {
        if (!coord::is_valid_local(x, y, z)) {
            return false;
        }
        set(x, y, z, voxel);
        return true;
    }

    // =============================================================================
    // RAW DATA ACCESS (For serialization, mesh generation, SIMD operations)
    // =============================================================================

    [[nodiscard]] Voxel* data() noexcept { return m_voxels.get(); }
    [[nodiscard]] const Voxel* data() const noexcept { return m_voxels.get(); }

    [[nodiscard]] std::uint32_t* raw_data() noexcept { 
        return reinterpret_cast<std::uint32_t*>(m_voxels.get()); 
    }
    [[nodiscard]] const std::uint32_t* raw_data() const noexcept { 
        return reinterpret_cast<const std::uint32_t*>(m_voxels.get()); 
    }

    // =============================================================================
    // STATE MANAGEMENT
    // =============================================================================

    [[nodiscard]] State state() const noexcept { return m_state; }
    void set_state(State state) noexcept { m_state = state; }

    [[nodiscard]] bool is_loaded() const noexcept { 
        return m_voxels != nullptr && m_state >= State::LOADED; 
    }

    [[nodiscard]] bool is_dirty() const noexcept { 
        return m_state == State::DIRTY || m_fully_dirty; 
    }

    [[nodiscard]] bool is_ready() const noexcept { 
        return m_state == State::READY; 
    }

    void mark_dirty() noexcept {
        if (m_state == State::LOADED || m_state == State::READY) {
            m_state = State::DIRTY;
        }
        m_fully_dirty = true;
    }

    void clear_dirty() noexcept {
        m_fully_dirty = false;
        if (m_state == State::DIRTY) {
            m_state = State::LOADED;
        }
    }

    // =============================================================================
    // POSITION
    // =============================================================================

    [[nodiscard]] const ChunkPosition& position() const noexcept { return m_position; }
    void set_position(ChunkPosition pos) noexcept { m_position = pos; }

    // =============================================================================
    // MEMORY MANAGEMENT
    // =============================================================================

    // Allocate voxel storage (called on demand)
    void allocate() {
        if (!m_voxels) {
            m_voxels.reset(allocate_voxels());
            std::memset(static_cast<void*>(m_voxels.get()), 0, DATA_SIZE_BYTES);
            m_state = State::LOADED;
            m_fully_dirty = true;
        }
    }

    // Release voxel storage (for unloading)
    void deallocate() noexcept {
        m_voxels.reset();
        m_state = State::UNLOADED;
        m_fully_dirty = false;
    }

    // =============================================================================
    // BULK OPERATIONS
    // =============================================================================

    // Fill entire chunk with a single voxel type
    void fill(Voxel voxel) noexcept {
        if (!m_voxels) return;
        
        // Optimized fill using 32-bit writes
        const std::uint32_t value = voxel.data;
        std::uint32_t* ptr = raw_data();
        
        for (std::uint32_t i = 0; i < VOLUME; ++i) {
            ptr[i] = value;
        }
        mark_dirty();
    }

    // Fill a region within the chunk
    void fill_region(LocalCoord x1, LocalCoord y1, LocalCoord z1,
                     LocalCoord x2, LocalCoord y2, LocalCoord z2,
                     Voxel voxel) noexcept {
        if (!m_voxels) return;

        // Clamp to chunk bounds
        x1 = x1 < 0 ? 0 : (x1 >= static_cast<LocalCoord>(SIZE_X) ? SIZE_X - 1 : x1);
        y1 = y1 < 0 ? 0 : (y1 >= static_cast<LocalCoord>(SIZE_Y) ? SIZE_Y - 1 : y1);
        z1 = z1 < 0 ? 0 : (z1 >= static_cast<LocalCoord>(SIZE_Z) ? SIZE_Z - 1 : z1);
        x2 = x2 < 0 ? 0 : (x2 >= static_cast<LocalCoord>(SIZE_X) ? SIZE_X - 1 : x2);
        y2 = y2 < 0 ? 0 : (y2 >= static_cast<LocalCoord>(SIZE_Y) ? SIZE_Y - 1 : y2);
        z2 = z2 < 0 ? 0 : (z2 >= static_cast<LocalCoord>(SIZE_Z) ? SIZE_Z - 1 : z2);

        for (LocalCoord x = x1; x <= x2; ++x) {
            for (LocalCoord z = z1; z <= z2; ++z) {
                for (LocalCoord y = y1; y <= y2; ++y) {
                    m_voxels[coord::to_index(x, y, z)] = voxel;
                }
            }
        }
        mark_dirty();
    }

    // Count non-air voxels (for optimization decisions)
    [[nodiscard]] std::uint32_t count_solid() const noexcept {
        if (!m_voxels) return 0;
        
        std::uint32_t count = 0;
        const Voxel* ptr = m_voxels.get();
        
        for (std::uint32_t i = 0; i < VOLUME; ++i) {
            if (!ptr[i].is_air()) {
                ++count;
            }
        }
        return count;
    }

    // Check if chunk is entirely air (skip meshing)
    [[nodiscard]] bool is_empty() const noexcept {
        if (!m_voxels) return true;
        
        const std::uint32_t* ptr = raw_data();
        for (std::uint32_t i = 0; i < VOLUME; ++i) {
            if ((ptr[i] & Voxel::TYPE_MASK) != 0) {
                return false;
            }
        }
        return true;
    }

    // Check if chunk is entirely solid (optimize face culling)
    [[nodiscard]] bool is_full() const noexcept {
        if (!m_voxels) return false;
        
        const std::uint32_t* ptr = raw_data();
        for (std::uint32_t i = 0; i < VOLUME; ++i) {
            if ((ptr[i] & Voxel::TYPE_MASK) == 0) {
                return false;
            }
        }
        return true;
    }

private:
    // Allocate 64-byte aligned voxel array
    [[nodiscard]] static Voxel* allocate_voxels() {
        void* ptr = ::operator new[](DATA_SIZE_BYTES, std::align_val_t{64});
        return static_cast<Voxel*>(ptr);
    }
};

// =============================================================================
// COMPILE-TIME VALIDATION
// =============================================================================
static_assert(alignof(Chunk) == 64, "Chunk must be 64-byte aligned");
static_assert(Chunk::VOLUME == 262144, "Chunk volume must be 64^3");
static_assert(Chunk::DATA_SIZE_BYTES == 1048576, "Chunk data must be 1 MiB");

} // namespace voxel
