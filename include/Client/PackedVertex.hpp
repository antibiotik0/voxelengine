// =============================================================================
// VOXEL ENGINE - PACKED VERTEX FORMAT
// 8-byte vertex structure for minimal VRAM bandwidth
// =============================================================================
#pragma once

#include <cstdint>
#include <type_traits>

namespace voxel::client {

// =============================================================================
// PACKED VERTEX (8 bytes total)
// Optimized for cache efficiency and VRAM bandwidth
//
// data1 layout (32 bits):
//   [Bits  0-5 ]: Position X (0-63, local chunk coordinate)
//   [Bits  6-11]: Position Y (0-63, local chunk coordinate)
//   [Bits 12-17]: Position Z (0-63, local chunk coordinate)
//   [Bits 18-20]: Normal index (0-5 for ±X, ±Y, ±Z)
//   [Bits 21-31]: UV/Texture index (0-2047)
//
// data2 layout (32 bits):
//   [Bits  0-15]: Voxel Type ID
//   [Bits 16-23]: Light level (packed sun + torch)
//   [Bits 24-31]: Ambient Occlusion (4 corners, 2 bits each)
// =============================================================================
struct PackedVertex {
    std::uint32_t data1;
    std::uint32_t data2;

    // Bit positions for data1
    static constexpr std::uint32_t POS_X_SHIFT = 0;
    static constexpr std::uint32_t POS_Y_SHIFT = 6;
    static constexpr std::uint32_t POS_Z_SHIFT = 12;
    static constexpr std::uint32_t NORMAL_SHIFT = 18;
    static constexpr std::uint32_t UV_INDEX_SHIFT = 21;

    static constexpr std::uint32_t POS_MASK = 0x3F;      // 6 bits
    static constexpr std::uint32_t NORMAL_MASK = 0x07;   // 3 bits
    static constexpr std::uint32_t UV_INDEX_MASK = 0x7FF; // 11 bits

    // Bit positions for data2
    static constexpr std::uint32_t VOXEL_ID_SHIFT = 0;
    static constexpr std::uint32_t LIGHT_SHIFT = 16;
    static constexpr std::uint32_t AO_SHIFT = 24;

    static constexpr std::uint32_t VOXEL_ID_MASK = 0xFFFF; // 16 bits
    static constexpr std::uint32_t LIGHT_MASK = 0xFF;      // 8 bits
    static constexpr std::uint32_t AO_MASK = 0xFF;         // 8 bits

    // Normal indices (face directions)
    enum NormalIndex : std::uint32_t {
        NORMAL_NEG_X = 0,  // -X (West)
        NORMAL_POS_X = 1,  // +X (East)
        NORMAL_NEG_Y = 2,  // -Y (Down)
        NORMAL_POS_Y = 3,  // +Y (Up)
        NORMAL_NEG_Z = 4,  // -Z (North)
        NORMAL_POS_Z = 5   // +Z (South)
    };

    // Default constructor
    constexpr PackedVertex() noexcept : data1(0), data2(0) {}

    // Full constructor
    constexpr PackedVertex(
        std::uint8_t pos_x, std::uint8_t pos_y, std::uint8_t pos_z,
        std::uint8_t normal, std::uint16_t uv_index,
        std::uint16_t voxel_id, std::uint8_t light, std::uint8_t ao
    ) noexcept
        : data1(
            (static_cast<std::uint32_t>(pos_x & POS_MASK) << POS_X_SHIFT) |
            (static_cast<std::uint32_t>(pos_y & POS_MASK) << POS_Y_SHIFT) |
            (static_cast<std::uint32_t>(pos_z & POS_MASK) << POS_Z_SHIFT) |
            (static_cast<std::uint32_t>(normal & NORMAL_MASK) << NORMAL_SHIFT) |
            (static_cast<std::uint32_t>(uv_index & UV_INDEX_MASK) << UV_INDEX_SHIFT)
        )
        , data2(
            (static_cast<std::uint32_t>(voxel_id)) |
            (static_cast<std::uint32_t>(light) << LIGHT_SHIFT) |
            (static_cast<std::uint32_t>(ao) << AO_SHIFT)
        )
    {}

    // Accessors
    [[nodiscard]] constexpr std::uint8_t pos_x() const noexcept {
        return static_cast<std::uint8_t>((data1 >> POS_X_SHIFT) & POS_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t pos_y() const noexcept {
        return static_cast<std::uint8_t>((data1 >> POS_Y_SHIFT) & POS_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t pos_z() const noexcept {
        return static_cast<std::uint8_t>((data1 >> POS_Z_SHIFT) & POS_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t normal() const noexcept {
        return static_cast<std::uint8_t>((data1 >> NORMAL_SHIFT) & NORMAL_MASK);
    }
    [[nodiscard]] constexpr std::uint16_t uv_index() const noexcept {
        return static_cast<std::uint16_t>((data1 >> UV_INDEX_SHIFT) & UV_INDEX_MASK);
    }
    [[nodiscard]] constexpr std::uint16_t voxel_id() const noexcept {
        return static_cast<std::uint16_t>((data2 >> VOXEL_ID_SHIFT) & VOXEL_ID_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t light() const noexcept {
        return static_cast<std::uint8_t>((data2 >> LIGHT_SHIFT) & LIGHT_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t ao() const noexcept {
        return static_cast<std::uint8_t>((data2 >> AO_SHIFT) & AO_MASK);
    }
};

// Compile-time validation
static_assert(sizeof(PackedVertex) == 8, "PackedVertex must be exactly 8 bytes");
static_assert(std::is_trivially_copyable_v<PackedVertex>, "PackedVertex must be trivially copyable");
static_assert(std::is_standard_layout_v<PackedVertex>, "PackedVertex must have standard layout");

// =============================================================================
// QUAD FACE (4 vertices forming a face)
// Used during mesh generation before conversion to triangles
// =============================================================================
struct QuadFace {
    PackedVertex vertices[4];

    constexpr QuadFace() noexcept = default;
    constexpr QuadFace(
        const PackedVertex& v0, const PackedVertex& v1,
        const PackedVertex& v2, const PackedVertex& v3
    ) noexcept : vertices{v0, v1, v2, v3} {}
};

static_assert(sizeof(QuadFace) == 32, "QuadFace must be 32 bytes");

} // namespace voxel::client
