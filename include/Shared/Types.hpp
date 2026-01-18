// =============================================================================
// VOXEL ENGINE - CORE TYPES AND CONSTANTS
// Sacred Interface: Immutable foundation of the engine
// =============================================================================
#pragma once

#include <cstdint>
#include <type_traits>
#include <bit>
#include <functional>

namespace voxel {

// =============================================================================
// WORLD COORDINATE SYSTEM
// Using int64_t for chunk coordinates to support ±10,000,000 world units
// =============================================================================
using ChunkCoord = std::int64_t;
using LocalCoord = std::int32_t;
using VoxelIndex = std::uint32_t;

// World boundary constants (±10 million units on X/Z axes)
inline constexpr ChunkCoord WORLD_BOUND_MAX =  10'000'000LL;
inline constexpr ChunkCoord WORLD_BOUND_MIN = -10'000'000LL;

// =============================================================================
// CHUNK DIMENSIONS (64^3 = 262,144 voxels per chunk)
// Power-of-two for bit-shift optimizations
// =============================================================================
inline constexpr std::uint32_t CHUNK_SIZE_X = 64;
inline constexpr std::uint32_t CHUNK_SIZE_Y = 64;
inline constexpr std::uint32_t CHUNK_SIZE_Z = 64;
inline constexpr std::uint32_t CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z; // 2^18

// Bit-shift constants for index calculation
inline constexpr std::uint32_t CHUNK_SHIFT_X = 12; // log2(64*64) for X contribution
inline constexpr std::uint32_t CHUNK_SHIFT_Z = 6;  // log2(64) for Z contribution
inline constexpr std::uint32_t CHUNK_MASK    = 63; // 0x3F for local coordinate masking

// =============================================================================
// VOXEL REPRESENTATION (32-bit POD Bitfield)
// Memory Layout:
//   [Bits  0-15]: Voxel Type ID (uint16_t) - 65,536 unique types
//   [Bits 16-19]: Sunlight Level (0-15)
//   [Bits 20-23]: Torchlight Level (0-15)
//   [Bits 24-31]: Metadata/Flags (rotation, fluid level, ECS link)
// =============================================================================
struct Voxel {
    std::uint32_t data;

    // Bit positions and masks
    static constexpr std::uint32_t TYPE_MASK       = 0x0000FFFF;
    static constexpr std::uint32_t TYPE_SHIFT      = 0;
    static constexpr std::uint32_t SUNLIGHT_MASK   = 0x000F0000;
    static constexpr std::uint32_t SUNLIGHT_SHIFT  = 16;
    static constexpr std::uint32_t TORCHLIGHT_MASK = 0x00F00000;
    static constexpr std::uint32_t TORCHLIGHT_SHIFT = 20;
    static constexpr std::uint32_t METADATA_MASK   = 0xFF000000;
    static constexpr std::uint32_t METADATA_SHIFT  = 24;

    // Default constructor - air block (type 0, no light, no metadata)
    constexpr Voxel() noexcept : data(0) {}

    // Explicit construction from raw bits
    constexpr explicit Voxel(std::uint32_t raw) noexcept : data(raw) {}

    // Construct with specific values
    constexpr Voxel(std::uint16_t type, std::uint8_t sunlight, 
                    std::uint8_t torchlight, std::uint8_t metadata) noexcept
        : data(static_cast<std::uint32_t>(type) |
               (static_cast<std::uint32_t>(sunlight & 0x0F) << SUNLIGHT_SHIFT) |
               (static_cast<std::uint32_t>(torchlight & 0x0F) << TORCHLIGHT_SHIFT) |
               (static_cast<std::uint32_t>(metadata) << METADATA_SHIFT)) {}

    // Accessors (no branching, pure bit operations)
    [[nodiscard]] constexpr std::uint16_t type_id() const noexcept {
        return static_cast<std::uint16_t>(data & TYPE_MASK);
    }

    [[nodiscard]] constexpr std::uint8_t sunlight() const noexcept {
        return static_cast<std::uint8_t>((data & SUNLIGHT_MASK) >> SUNLIGHT_SHIFT);
    }

    [[nodiscard]] constexpr std::uint8_t torchlight() const noexcept {
        return static_cast<std::uint8_t>((data & TORCHLIGHT_MASK) >> TORCHLIGHT_SHIFT);
    }

    [[nodiscard]] constexpr std::uint8_t metadata() const noexcept {
        return static_cast<std::uint8_t>((data & METADATA_MASK) >> METADATA_SHIFT);
    }

    // Combined light level (max of sun and torch)
    [[nodiscard]] constexpr std::uint8_t light_level() const noexcept {
        const auto sun = sunlight();
        const auto torch = torchlight();
        return sun > torch ? sun : torch;
    }

    // Mutators (return new Voxel for immutability option, or modify in-place)
    constexpr void set_type(std::uint16_t type) noexcept {
        data = (data & ~TYPE_MASK) | static_cast<std::uint32_t>(type);
    }

    constexpr void set_sunlight(std::uint8_t level) noexcept {
        data = (data & ~SUNLIGHT_MASK) | 
               (static_cast<std::uint32_t>(level & 0x0F) << SUNLIGHT_SHIFT);
    }

    constexpr void set_torchlight(std::uint8_t level) noexcept {
        data = (data & ~TORCHLIGHT_MASK) | 
               (static_cast<std::uint32_t>(level & 0x0F) << TORCHLIGHT_SHIFT);
    }

    constexpr void set_metadata(std::uint8_t meta) noexcept {
        data = (data & ~METADATA_MASK) | 
               (static_cast<std::uint32_t>(meta) << METADATA_SHIFT);
    }

    // Predicate for empty/air voxels
    [[nodiscard]] constexpr bool is_air() const noexcept {
        return type_id() == 0;
    }

    [[nodiscard]] constexpr bool is_opaque() const noexcept {
        // Type IDs 1-255 are opaque by default (configurable via registry)
        return type_id() != 0 && type_id() < 256;
    }

    // Comparison operators
    [[nodiscard]] constexpr bool operator==(const Voxel& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const Voxel& other) const noexcept = default;
};

// =============================================================================
// COMPILE-TIME VALIDATION
// =============================================================================
static_assert(sizeof(Voxel) == 4, "Voxel must be exactly 32 bits");
static_assert(std::is_trivially_copyable_v<Voxel>, "Voxel must be trivially copyable");
static_assert(std::is_trivially_destructible_v<Voxel>, "Voxel must be trivially destructible");
static_assert(std::is_standard_layout_v<Voxel>, "Voxel must have standard layout");

// =============================================================================
// COMMON VOXEL TYPE IDS
// =============================================================================
namespace VoxelType {
    inline constexpr std::uint16_t AIR     = 0;
    inline constexpr std::uint16_t STONE   = 1;
    inline constexpr std::uint16_t DIRT    = 2;
    inline constexpr std::uint16_t GRASS   = 3;
    inline constexpr std::uint16_t WATER   = 4;
    inline constexpr std::uint16_t SAND    = 5;
    inline constexpr std::uint16_t WOOD    = 6;
    inline constexpr std::uint16_t LEAVES  = 7;
    inline constexpr std::uint16_t GLASS   = 8;
    inline constexpr std::uint16_t LIGHT   = 9;  // Light-emitting block
    // 10-65535: User-defined types
}

// =============================================================================
// COORDINATE UTILITIES (Bit-shift only, no multiplication/division)
// =============================================================================
namespace coord {

    // Convert local (x, y, z) to flat array index using bit-shifts only
    // Index = (x << 12) | (z << 6) | y  (Y-up, column-major for cache locality)
    [[nodiscard]] constexpr VoxelIndex to_index(LocalCoord x, LocalCoord y, LocalCoord z) noexcept {
        return static_cast<VoxelIndex>(
            (static_cast<std::uint32_t>(x & CHUNK_MASK) << CHUNK_SHIFT_X) |
            (static_cast<std::uint32_t>(z & CHUNK_MASK) << CHUNK_SHIFT_Z) |
            static_cast<std::uint32_t>(y & CHUNK_MASK)
        );
    }

    // Extract local coordinates from flat index
    [[nodiscard]] constexpr LocalCoord index_to_x(VoxelIndex index) noexcept {
        return static_cast<LocalCoord>((index >> CHUNK_SHIFT_X) & CHUNK_MASK);
    }

    [[nodiscard]] constexpr LocalCoord index_to_y(VoxelIndex index) noexcept {
        return static_cast<LocalCoord>(index & CHUNK_MASK);
    }

    [[nodiscard]] constexpr LocalCoord index_to_z(VoxelIndex index) noexcept {
        return static_cast<LocalCoord>((index >> CHUNK_SHIFT_Z) & CHUNK_MASK);
    }

    // World coordinate to chunk coordinate (bit-shift division by 64)
    [[nodiscard]] constexpr ChunkCoord world_to_chunk(ChunkCoord world) noexcept {
        // Arithmetic right shift for negative coordinates
        return world >> 6; // Equivalent to floor(world / 64)
    }

    // World coordinate to local coordinate within chunk
    [[nodiscard]] constexpr LocalCoord world_to_local(ChunkCoord world) noexcept {
        return static_cast<LocalCoord>(world & CHUNK_MASK);
    }

    // Chunk coordinate to world coordinate (bit-shift multiplication by 64)
    [[nodiscard]] constexpr ChunkCoord chunk_to_world(ChunkCoord chunk) noexcept {
        return chunk << 6;
    }

    // Validate coordinates are within chunk bounds
    [[nodiscard]] constexpr bool is_valid_local(LocalCoord x, LocalCoord y, LocalCoord z) noexcept {
        return (static_cast<std::uint32_t>(x) < CHUNK_SIZE_X) &&
               (static_cast<std::uint32_t>(y) < CHUNK_SIZE_Y) &&
               (static_cast<std::uint32_t>(z) < CHUNK_SIZE_Z);
    }

    // Validate chunk coordinates are within world bounds
    [[nodiscard]] constexpr bool is_valid_chunk(ChunkCoord x, ChunkCoord z) noexcept {
        return (x >= (WORLD_BOUND_MIN >> 6)) && (x <= (WORLD_BOUND_MAX >> 6)) &&
               (z >= (WORLD_BOUND_MIN >> 6)) && (z <= (WORLD_BOUND_MAX >> 6));
    }

} // namespace coord

// =============================================================================
// CHUNK POSITION (World-space chunk identifier)
// =============================================================================
struct ChunkPosition {
    ChunkCoord x;
    ChunkCoord y;
    ChunkCoord z;

    constexpr ChunkPosition() noexcept : x(0), y(0), z(0) {}
    constexpr ChunkPosition(ChunkCoord cx, ChunkCoord cy, ChunkCoord cz) noexcept 
        : x(cx), y(cy), z(cz) {}

    [[nodiscard]] constexpr bool operator==(const ChunkPosition& other) const noexcept = default;

    // Hash support for use in containers
    [[nodiscard]] constexpr std::size_t hash() const noexcept {
        // FNV-1a inspired hash
        std::size_t h = 14695981039346656037ULL;
        h ^= static_cast<std::size_t>(x);
        h *= 1099511628211ULL;
        h ^= static_cast<std::size_t>(y);
        h *= 1099511628211ULL;
        h ^= static_cast<std::size_t>(z);
        h *= 1099511628211ULL;
        return h;
    }
};

// Custom hash functor for ChunkPosition (used in unordered_map)
struct ChunkPositionHash {
    [[nodiscard]] std::size_t operator()(const ChunkPosition& pos) const noexcept {
        return pos.hash();
    }
};

} // namespace voxel

// std::hash specialization for ChunkPosition
namespace std {
    template<>
    struct hash<voxel::ChunkPosition> {
        [[nodiscard]] std::size_t operator()(const voxel::ChunkPosition& pos) const noexcept {
            return pos.hash();
        }
    };
}
