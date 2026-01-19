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
// Supports Greedy Meshing with GL_REPEAT texture wrapping
//
// data1 layout (32 bits):
//   [Bits  0-6 ]: Position X (0-64, local chunk coordinate + edge)
//   [Bits  7-13]: Position Y (0-64, local chunk coordinate + edge)
//   [Bits 14-20]: Position Z (0-64, local chunk coordinate + edge)
//   [Bits 21-23]: Normal index (0-5 for ±X, ±Y, ±Z)
//   [Bits 24-31]: Texture layer index (0-255, for sampler2DArray)
//
// data2 layout (32 bits):
//   [Bits  0-7 ]: UV U coordinate (0-255, for greedy mesh stretching)
//   [Bits  8-15]: UV V coordinate (0-255, for greedy mesh stretching)
//   [Bits 16-23]: Light level (packed sun + torch)
//   [Bits 24-31]: Ambient Occlusion (4 corners, 2 bits each)
// =============================================================================
struct PackedVertex {
    std::uint32_t data1;
    std::uint32_t data2;

    // Bit positions for data1 (7-bit positions for 0-64 range)
    static constexpr std::uint32_t POS_X_SHIFT = 0;
    static constexpr std::uint32_t POS_Y_SHIFT = 7;
    static constexpr std::uint32_t POS_Z_SHIFT = 14;
    static constexpr std::uint32_t NORMAL_SHIFT = 21;
    static constexpr std::uint32_t TEX_LAYER_SHIFT = 24;

    static constexpr std::uint32_t POS_MASK = 0x7F;       // 7 bits (0-127)
    static constexpr std::uint32_t NORMAL_MASK = 0x07;    // 3 bits
    static constexpr std::uint32_t TEX_LAYER_MASK = 0xFF; // 8 bits

    // Bit positions for data2
    static constexpr std::uint32_t UV_U_SHIFT = 0;
    static constexpr std::uint32_t UV_V_SHIFT = 8;
    static constexpr std::uint32_t LIGHT_SHIFT = 16;
    static constexpr std::uint32_t AO_SHIFT = 24;

    static constexpr std::uint32_t UV_MASK = 0xFF;    // 8 bits
    static constexpr std::uint32_t LIGHT_MASK = 0xFF; // 8 bits
    static constexpr std::uint32_t AO_MASK = 0xFF;    // 8 bits

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

    // Full constructor with texture layer and UV coordinates
    constexpr PackedVertex(
        std::uint8_t pos_x, std::uint8_t pos_y, std::uint8_t pos_z,
        std::uint8_t normal, std::uint8_t tex_layer,
        std::uint8_t uv_u, std::uint8_t uv_v,
        std::uint8_t light, std::uint8_t ao
    ) noexcept
        : data1(
            (static_cast<std::uint32_t>(pos_x & POS_MASK) << POS_X_SHIFT) |
            (static_cast<std::uint32_t>(pos_y & POS_MASK) << POS_Y_SHIFT) |
            (static_cast<std::uint32_t>(pos_z & POS_MASK) << POS_Z_SHIFT) |
            (static_cast<std::uint32_t>(normal & NORMAL_MASK) << NORMAL_SHIFT) |
            (static_cast<std::uint32_t>(tex_layer & TEX_LAYER_MASK) << TEX_LAYER_SHIFT)
        )
        , data2(
            (static_cast<std::uint32_t>(uv_u)) |
            (static_cast<std::uint32_t>(uv_v) << UV_V_SHIFT) |
            (static_cast<std::uint32_t>(light) << LIGHT_SHIFT) |
            (static_cast<std::uint32_t>(ao) << AO_SHIFT)
        )
    {}

    // Legacy constructor for backward compatibility (converts voxel_id to tex_layer)
    constexpr PackedVertex(
        std::uint8_t pos_x, std::uint8_t pos_y, std::uint8_t pos_z,
        std::uint8_t normal, std::uint16_t voxel_id_or_tex,
        std::uint16_t /* ignored */, std::uint8_t light, std::uint8_t ao
    ) noexcept
        : data1(
            (static_cast<std::uint32_t>(pos_x & POS_MASK) << POS_X_SHIFT) |
            (static_cast<std::uint32_t>(pos_y & POS_MASK) << POS_Y_SHIFT) |
            (static_cast<std::uint32_t>(pos_z & POS_MASK) << POS_Z_SHIFT) |
            (static_cast<std::uint32_t>(normal & NORMAL_MASK) << NORMAL_SHIFT) |
            (static_cast<std::uint32_t>(voxel_id_or_tex & TEX_LAYER_MASK) << TEX_LAYER_SHIFT)
        )
        , data2(
            (static_cast<std::uint32_t>(0)) |           // UV U = 0
            (static_cast<std::uint32_t>(0) << UV_V_SHIFT) | // UV V = 0
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
    [[nodiscard]] constexpr std::uint8_t tex_layer() const noexcept {
        return static_cast<std::uint8_t>((data1 >> TEX_LAYER_SHIFT) & TEX_LAYER_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t uv_u() const noexcept {
        return static_cast<std::uint8_t>((data2 >> UV_U_SHIFT) & UV_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t uv_v() const noexcept {
        return static_cast<std::uint8_t>((data2 >> UV_V_SHIFT) & UV_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t light() const noexcept {
        return static_cast<std::uint8_t>((data2 >> LIGHT_SHIFT) & LIGHT_MASK);
    }
    [[nodiscard]] constexpr std::uint8_t ao() const noexcept {
        return static_cast<std::uint8_t>((data2 >> AO_SHIFT) & AO_MASK);
    }
    
    // Setters for UV (useful for greedy meshing)
    constexpr void set_uv(std::uint8_t u, std::uint8_t v) noexcept {
        data2 = (data2 & 0xFFFF0000u) | 
                (static_cast<std::uint32_t>(u)) |
                (static_cast<std::uint32_t>(v) << UV_V_SHIFT);
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
