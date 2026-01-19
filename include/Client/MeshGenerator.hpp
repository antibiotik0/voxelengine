// =============================================================================
// VOXEL ENGINE - MESH GENERATOR
// Greedy Meshing with Face Culling for optimal geometry generation
// =============================================================================
#pragma once

#include "PackedVertex.hpp"
#include "ChunkMesh.hpp"
#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"

#include <array>
#include <cstdint>
#include <functional>

namespace voxel::client {

// =============================================================================
// NEIGHBOR CHUNK ACCESS CALLBACK
// Used to check voxels in adjacent chunks for proper face culling
// =============================================================================
using NeighborAccessor = std::function<Voxel(ChunkCoord, ChunkCoord, ChunkCoord)>;

// =============================================================================
// MESH GENERATOR CONFIGURATION
// =============================================================================
struct MeshGenConfig {
    bool enable_greedy_meshing = true;
    bool enable_ao = true;           // Ambient occlusion calculation
    bool enable_face_culling = true; // Cull internal faces
    bool generate_water_mesh = false; // Separate water mesh (future)
};

// =============================================================================
// MESH GENERATOR
// Implements greedy meshing algorithm with face culling
// =============================================================================
class MeshGenerator {
public:
    // Chunk dimensions
    static constexpr std::uint32_t SIZE = CHUNK_SIZE_X; // 64
    static constexpr std::uint32_t SIZE_SQ = SIZE * SIZE;
    static constexpr std::uint32_t SIZE_CUBE = SIZE * SIZE * SIZE;

    // Face direction enum
    enum Face : std::uint8_t {
        FACE_NEG_X = 0,
        FACE_POS_X = 1,
        FACE_NEG_Y = 2,
        FACE_POS_Y = 3,
        FACE_NEG_Z = 4,
        FACE_POS_Z = 5,
        FACE_COUNT = 6
    };

    MeshGenerator();
    explicit MeshGenerator(MeshGenConfig config);
    ~MeshGenerator() = default;

    // Non-copyable (internal buffers)
    MeshGenerator(const MeshGenerator&) = delete;
    MeshGenerator& operator=(const MeshGenerator&) = delete;
    MeshGenerator(MeshGenerator&&) = default;
    MeshGenerator& operator=(MeshGenerator&&) = default;

    // =============================================================================
    // MAIN GENERATION INTERFACE
    // =============================================================================

    // Generate mesh for a single chunk
    // neighbor_accessor provides voxel data from adjacent chunks for proper culling
    void generate(
        const Chunk& chunk,
        ChunkMesh& out_mesh,
        const NeighborAccessor& neighbor_accessor = nullptr
    );

    // Generate mesh without neighbor data (internal faces only)
    void generate_simple(const Chunk& chunk, ChunkMesh& out_mesh);

    // =============================================================================
    // CONFIGURATION
    // =============================================================================

    void set_config(MeshGenConfig config) noexcept { m_config = config; }
    [[nodiscard]] const MeshGenConfig& config() const noexcept { return m_config; }

    // =============================================================================
    // STATISTICS
    // =============================================================================

    [[nodiscard]] std::uint32_t last_faces_generated() const noexcept { return m_stats_faces; }
    [[nodiscard]] std::uint32_t last_faces_culled() const noexcept { return m_stats_culled; }

private:
    // =============================================================================
    // INTERNAL TYPES
    // =============================================================================

    // Occupancy mask for greedy meshing (64x64 = 4096 bits = 64 uint64_t)
    using SliceMask = std::array<std::uint64_t, SIZE>;

    // Face data for greedy meshing
    struct FaceData {
        std::uint16_t voxel_type;
        std::uint8_t light;
        std::uint8_t ao;
        std::uint8_t fluid_level;  // 0-8, for lowering fluid top faces

        [[nodiscard]] bool operator==(const FaceData& other) const noexcept {
            return voxel_type == other.voxel_type && 
                   light == other.light && 
                   ao == other.ao &&
                   fluid_level == other.fluid_level;
        }
    };

    // 2D slice for greedy meshing (64x64)
    using FaceSlice = std::array<FaceData, SIZE_SQ>;

    // =============================================================================
    // GREEDY MESHING IMPLEMENTATION
    // =============================================================================

    // Build face visibility mask for a given axis direction
    void build_face_masks(
        const Chunk& chunk,
        Face face,
        const NeighborAccessor& neighbor_accessor
    );

    // Perform greedy meshing on a 2D slice
    void greedy_mesh_slice(
        std::uint32_t slice_coord,
        Face face,
        ChunkMesh& out_mesh
    );

    // Check if voxel at position is opaque
    [[nodiscard]] bool is_opaque(const Chunk& chunk, LocalCoord x, LocalCoord y, LocalCoord z) const;

    // Get voxel with neighbor fallback
    [[nodiscard]] Voxel get_voxel_or_neighbor(
        const Chunk& chunk,
        LocalCoord x, LocalCoord y, LocalCoord z,
        const NeighborAccessor& neighbor_accessor
    ) const;

    // Calculate ambient occlusion for a vertex
    [[nodiscard]] std::uint8_t calculate_ao(
        const Chunk& chunk,
        LocalCoord x, LocalCoord y, LocalCoord z,
        Face face, std::uint8_t corner,
        const NeighborAccessor& neighbor_accessor
    ) const;

    // Add a quad to the mesh
    void add_face_quad(
        ChunkMesh& mesh,
        std::uint32_t x, std::uint32_t y, std::uint32_t z,
        std::uint32_t width, std::uint32_t height,
        Face face,
        const FaceData& data
    );

private:
    MeshGenConfig m_config;

    // Working buffers (reused across generate calls)
    std::array<FaceSlice, SIZE> m_face_slices;   // 64 slices of 64x64 faces
    std::array<SliceMask, SIZE> m_visited_masks; // Visited tracking for greedy

    // Statistics
    std::uint32_t m_stats_faces = 0;
    std::uint32_t m_stats_culled = 0;
};

} // namespace voxel::client
