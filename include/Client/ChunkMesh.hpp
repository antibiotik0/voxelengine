// =============================================================================
// VOXEL ENGINE - CHUNK MESH
// Container for generated mesh data ready for GPU upload
// =============================================================================
#pragma once

#include "PackedVertex.hpp"
#include "Shared/Types.hpp"

#include <vector>
#include <cstdint>

namespace voxel::client {

// =============================================================================
// CHUNK MESH
// Contains vertex and index data for a single chunk
// =============================================================================
struct ChunkMesh {
    // Vertex data (packed 8-byte vertices)
    std::vector<PackedVertex> vertices;

    // Index data (32-bit indices for large meshes)
    std::vector<std::uint32_t> indices;

    // Chunk world position (for origin-relative rendering)
    ChunkPosition position;

    // Mesh statistics
    std::uint32_t quad_count = 0;     // Number of quads (faces)
    std::uint32_t triangle_count = 0; // Number of triangles (quad_count * 2)

    // GPU buffer handles (set by renderer)
    std::uint32_t vao = 0;
    std::uint32_t vbo = 0;
    std::uint32_t ebo = 0;

    // State flags
    bool is_empty = true;
    bool is_uploaded = false;
    bool needs_update = false;

    ChunkMesh() = default;
    explicit ChunkMesh(ChunkPosition pos) : position(pos) {}

    // Move-only (large data)
    ChunkMesh(const ChunkMesh&) = delete;
    ChunkMesh& operator=(const ChunkMesh&) = delete;
    ChunkMesh(ChunkMesh&&) noexcept = default;
    ChunkMesh& operator=(ChunkMesh&&) noexcept = default;

    // Clear mesh data
    void clear() noexcept {
        vertices.clear();
        indices.clear();
        quad_count = 0;
        triangle_count = 0;
        is_empty = true;
        needs_update = true;
    }

    // Reserve capacity for expected face count
    void reserve(std::size_t expected_quads) {
        vertices.reserve(expected_quads * 4);  // 4 vertices per quad
        indices.reserve(expected_quads * 6);   // 6 indices per quad (2 triangles)
    }

    // Add a quad face (4 vertices, 6 indices)
    void add_quad(const PackedVertex& v0, const PackedVertex& v1,
                  const PackedVertex& v2, const PackedVertex& v3) {
        const auto base_index = static_cast<std::uint32_t>(vertices.size());

        // Add 4 vertices
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        // Add 6 indices (2 triangles, CCW winding)
        // Triangle 1: 0-1-2
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 1);
        indices.push_back(base_index + 2);

        // Triangle 2: 2-3-0
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 3);
        indices.push_back(base_index + 0);

        ++quad_count;
        triangle_count = quad_count * 2;
        is_empty = false;
    }

    // Memory usage in bytes
    [[nodiscard]] std::size_t memory_usage() const noexcept {
        return vertices.size() * sizeof(PackedVertex) +
               indices.size() * sizeof(std::uint32_t);
    }
};

// =============================================================================
// DRAW COMMAND (for glMultiDrawElementsIndirect)
// =============================================================================
struct DrawElementsIndirectCommand {
    std::uint32_t count;          // Number of indices
    std::uint32_t instance_count; // Number of instances
    std::uint32_t first_index;    // Offset in index buffer
    std::int32_t  base_vertex;    // Offset in vertex buffer
    std::uint32_t base_instance;  // First instance ID

    constexpr DrawElementsIndirectCommand() noexcept
        : count(0), instance_count(1), first_index(0), base_vertex(0), base_instance(0) {}

    constexpr DrawElementsIndirectCommand(
        std::uint32_t idx_count, std::uint32_t first_idx,
        std::int32_t base_vtx, std::uint32_t base_inst = 0
    ) noexcept
        : count(idx_count), instance_count(1), first_index(first_idx),
          base_vertex(base_vtx), base_instance(base_inst) {}
};

static_assert(sizeof(DrawElementsIndirectCommand) == 20, "DrawElementsIndirectCommand must be 20 bytes");

} // namespace voxel::client
