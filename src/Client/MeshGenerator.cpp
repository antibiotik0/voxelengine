// =============================================================================
// VOXEL ENGINE - MESH GENERATOR IMPLEMENTATION
// Greedy Meshing with Face Culling and Texture Array support
// =============================================================================

#include "Client/MeshGenerator.hpp"
#include "Shared/BlockRegistry.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace voxel::client {

// =============================================================================
// CONSTRUCTION
// =============================================================================

MeshGenerator::MeshGenerator()
    : m_config{}
{}

MeshGenerator::MeshGenerator(MeshGenConfig config)
    : m_config(config)
{}

// =============================================================================
// MAIN GENERATION INTERFACE
// =============================================================================

void MeshGenerator::generate(
    const Chunk& chunk,
    ChunkMesh& out_mesh,
    const NeighborAccessor& neighbor_accessor
) {
    // Reset output mesh
    out_mesh.clear();
    out_mesh.position = chunk.position();
    out_mesh.reserve(SIZE_SQ * 6); // Worst case: all faces visible

    // Reset statistics
    m_stats_faces = 0;
    m_stats_culled = 0;

    // Early exit for empty chunks
    if (!chunk.is_loaded() || chunk.is_empty()) {
        return;
    }

    // Generate mesh for each face direction
    for (std::uint8_t face = 0; face < FACE_COUNT; ++face) {
        const Face f = static_cast<Face>(face);

        // Build face visibility data for this direction
        build_face_masks(chunk, f, neighbor_accessor);

        // Perform greedy meshing on each slice
        for (std::uint32_t slice = 0; slice < SIZE; ++slice) {
            greedy_mesh_slice(slice, f, out_mesh);
        }
    }

    out_mesh.is_empty = (out_mesh.quad_count == 0);
    out_mesh.needs_update = true;
}

void MeshGenerator::generate_simple(const Chunk& chunk, ChunkMesh& out_mesh) {
    generate(chunk, out_mesh, nullptr);
}

// =============================================================================
// FACE MASK BUILDING
// =============================================================================

void MeshGenerator::build_face_masks(
    const Chunk& chunk,
    Face face,
    const NeighborAccessor& neighbor_accessor
) {
    // Clear working buffers
    for (auto& slice : m_face_slices) {
        std::memset(slice.data(), 0, sizeof(FaceSlice));
    }
    for (auto& mask : m_visited_masks) {
        mask.fill(0);
    }

    // Direction vectors for each face
    static constexpr std::int32_t dir_x[] = {-1, 1, 0, 0, 0, 0};
    static constexpr std::int32_t dir_y[] = {0, 0, -1, 1, 0, 0};
    static constexpr std::int32_t dir_z[] = {0, 0, 0, 0, -1, 1};

    const std::int32_t dx = dir_x[face];
    const std::int32_t dy = dir_y[face];
    const std::int32_t dz = dir_z[face];

    // Iterate through all voxels
    for (std::uint32_t x = 0; x < SIZE; ++x) {
        for (std::uint32_t y = 0; y < SIZE; ++y) {
            for (std::uint32_t z = 0; z < SIZE; ++z) {
                // Get current voxel
                const Voxel voxel = chunk.get(
                    static_cast<LocalCoord>(x),
                    static_cast<LocalCoord>(y),
                    static_cast<LocalCoord>(z)
                );

                // Skip air blocks
                if (voxel.is_air()) {
                    ++m_stats_culled;
                    continue;
                }

                // Check neighbor in face direction
                const std::int32_t nx = static_cast<std::int32_t>(x) + dx;
                const std::int32_t ny = static_cast<std::int32_t>(y) + dy;
                const std::int32_t nz = static_cast<std::int32_t>(z) + dz;

                bool should_cull = false;
                
                // Get current block properties
                const auto& current_props = BlockRegistry::instance().get(voxel.type_id());

                // Check if neighbor is within chunk
                if (nx >= 0 && nx < static_cast<std::int32_t>(SIZE) &&
                    ny >= 0 && ny < static_cast<std::int32_t>(SIZE) &&
                    nz >= 0 && nz < static_cast<std::int32_t>(SIZE)) {
                    // Neighbor is in same chunk
                    const Voxel neighbor = chunk.get(
                        static_cast<LocalCoord>(nx),
                        static_cast<LocalCoord>(ny),
                        static_cast<LocalCoord>(nz)
                    );
                    
                    // Get neighbor properties from BlockRegistry
                    const auto& neighbor_props = BlockRegistry::instance().get(neighbor.type_id());
                    
                    // WATER/FLUID CULLING RULES:
                    // Show face if:
                    //   - Neighbor is AIR
                    //   - Neighbor is a different block type
                    //   - Neighbor is same fluid but has lower liquid_level
                    // Cull face if:
                    //   - Neighbor is opaque AND current is opaque
                    //   - Neighbor is same fluid with >= liquid_level
                    if (!neighbor.is_air()) {
                        if (!neighbor_props.is_transparent && !current_props.is_transparent) {
                            // Both opaque - cull
                            should_cull = true;
                        } else if (current_props.is_fluid && neighbor.type_id() == voxel.type_id()) {
                            // Same fluid type - cull only if neighbor has >= fluid level
                            std::uint8_t current_level = voxel.fluid_level();
                            std::uint8_t neighbor_level = neighbor.fluid_level();
                            if (current_level == 0) current_level = Voxel::FLUID_LEVEL_FULL;
                            if (neighbor_level == 0) neighbor_level = Voxel::FLUID_LEVEL_FULL;
                            should_cull = (neighbor_level >= current_level);
                        } else if (neighbor_props.is_transparent && current_props.is_transparent &&
                                   neighbor.type_id() == voxel.type_id()) {
                            // Same non-fluid transparent type - cull internal faces
                            should_cull = !current_props.render_all_faces;
                        }
                        // Different types or opaque behind transparent - don't cull
                    }
                } else if (m_config.enable_face_culling && neighbor_accessor) {
                    // Neighbor is in adjacent chunk
                    const ChunkPosition& pos = chunk.position();
                    ChunkCoord world_x = coord::chunk_to_world(pos.x) + nx;
                    ChunkCoord world_y = coord::chunk_to_world(pos.y) + ny;
                    ChunkCoord world_z = coord::chunk_to_world(pos.z) + nz;

                    const Voxel neighbor = neighbor_accessor(world_x, world_y, world_z);
                    const auto& neighbor_props = BlockRegistry::instance().get(neighbor.type_id());
                    
                    if (!neighbor.is_air()) {
                        if (!neighbor_props.is_transparent && !current_props.is_transparent) {
                            should_cull = true;
                        } else if (current_props.is_fluid && neighbor.type_id() == voxel.type_id()) {
                            // Same fluid type - cull only if neighbor has >= fluid level
                            std::uint8_t current_level = voxel.fluid_level();
                            std::uint8_t neighbor_level = neighbor.fluid_level();
                            if (current_level == 0) current_level = Voxel::FLUID_LEVEL_FULL;
                            if (neighbor_level == 0) neighbor_level = Voxel::FLUID_LEVEL_FULL;
                            should_cull = (neighbor_level >= current_level);
                        } else if (neighbor_props.is_transparent && current_props.is_transparent &&
                                   neighbor.type_id() == voxel.type_id()) {
                            should_cull = !current_props.render_all_faces;
                        }
                    }
                }

                // Face culling
                if (m_config.enable_face_culling && should_cull) {
                    ++m_stats_culled;
                    continue;
                }

                // Determine slice coordinate based on face direction
                std::uint32_t slice_coord, u_coord, v_coord;
                switch (face) {
                    case FACE_NEG_X:
                    case FACE_POS_X:
                        slice_coord = x;
                        u_coord = z;
                        v_coord = y;
                        break;
                    case FACE_NEG_Y:
                    case FACE_POS_Y:
                        slice_coord = y;
                        u_coord = x;
                        v_coord = z;
                        break;
                    case FACE_NEG_Z:
                    case FACE_POS_Z:
                    default:
                        slice_coord = z;
                        u_coord = x;
                        v_coord = y;
                        break;
                }

                // Store face data
                const std::uint32_t slice_index = v_coord * SIZE + u_coord;
                FaceData& face_data = m_face_slices[slice_coord][slice_index];
                face_data.voxel_type = voxel.type_id();
                // Force full brightness until proper lighting is implemented
                face_data.light = 255;
                face_data.ao = 0; // TODO: Calculate AO
                
                // Store fluid level for water/lava height lowering
                if (current_props.is_fluid) {
                    face_data.fluid_level = voxel.fluid_level();
                    // Default to full if not set
                    if (face_data.fluid_level == 0) {
                        face_data.fluid_level = Voxel::FLUID_LEVEL_FULL;
                    }
                } else {
                    face_data.fluid_level = 0;
                }

                ++m_stats_faces;
            }
        }
    }
}

// =============================================================================
// GREEDY MESHING
// =============================================================================

void MeshGenerator::greedy_mesh_slice(
    std::uint32_t slice_coord,
    Face face,
    ChunkMesh& out_mesh
) {
    FaceSlice& slice = m_face_slices[slice_coord];
    SliceMask& visited = m_visited_masks[slice_coord];

    // Reset visited mask
    visited.fill(0);

    for (std::uint32_t v = 0; v < SIZE; ++v) {
        for (std::uint32_t u = 0; u < SIZE; ++u) {
            const std::uint32_t index = v * SIZE + u;

            // Skip if already visited or empty
            if ((visited[v] & (1ULL << u)) != 0) continue;

            const FaceData& start_data = slice[index];
            if (start_data.voxel_type == 0) continue;

            // Greedy expansion
            std::uint32_t width = 1;
            std::uint32_t height = 1;

            if (m_config.enable_greedy_meshing) {
                // Expand width (u direction)
                while (u + width < SIZE) {
                    const std::uint32_t next_index = v * SIZE + (u + width);
                    if ((visited[v] & (1ULL << (u + width))) != 0) break;
                    if (!(slice[next_index] == start_data)) break;
                    ++width;
                }

                // Expand height (v direction)
                bool can_expand = true;
                while (can_expand && v + height < SIZE) {
                    for (std::uint32_t du = 0; du < width; ++du) {
                        const std::uint32_t check_index = (v + height) * SIZE + (u + du);
                        if ((visited[v + height] & (1ULL << (u + du))) != 0) {
                            can_expand = false;
                            break;
                        }
                        if (!(slice[check_index] == start_data)) {
                            can_expand = false;
                            break;
                        }
                    }
                    if (can_expand) ++height;
                }
            }

            // Mark as visited
            for (std::uint32_t dv = 0; dv < height; ++dv) {
                for (std::uint32_t du = 0; du < width; ++du) {
                    visited[v + dv] |= (1ULL << (u + du));
                }
            }

            // Convert UV coordinates back to XYZ
            std::uint32_t x, y, z;
            std::uint32_t w, h;

            switch (face) {
                case FACE_NEG_X:
                case FACE_POS_X:
                    x = slice_coord;
                    z = u;
                    y = v;
                    w = width;
                    h = height;
                    break;
                case FACE_NEG_Y:
                case FACE_POS_Y:
                    x = u;
                    y = slice_coord;
                    z = v;
                    w = width;
                    h = height;
                    break;
                case FACE_NEG_Z:
                case FACE_POS_Z:
                default:
                    x = u;
                    y = v;
                    z = slice_coord;
                    w = width;
                    h = height;
                    break;
            }

            // Add quad to mesh
            add_face_quad(out_mesh, x, y, z, w, h, face, start_data);
        }
    }
}

// =============================================================================
// QUAD GENERATION
// =============================================================================

void MeshGenerator::add_face_quad(
    ChunkMesh& mesh,
    std::uint32_t x, std::uint32_t y, std::uint32_t z,
    std::uint32_t width, std::uint32_t height,
    Face face,
    const FaceData& data
) {
    // Get block properties from BlockRegistry
    const auto& props = BlockRegistry::instance().get(data.voxel_type);
    
    // Determine texture layer based on face direction
    std::uint8_t tex_layer;
    switch (face) {
        case FACE_POS_Y: // Top face
            tex_layer = props.texture_top;
            break;
        case FACE_NEG_Y: // Bottom face
            tex_layer = props.texture_bottom;
            break;
        default: // Side faces
            tex_layer = props.texture_side;
            break;
    }
    
    // Vertex positions for each face (CCW winding)
    std::uint8_t x0, y0, z0;
    std::uint8_t x1, y1, z1;
    std::uint8_t x2, y2, z2;
    std::uint8_t x3, y3, z3;

    const auto bx = static_cast<std::uint8_t>(x);
    const auto by = static_cast<std::uint8_t>(y);
    const auto bz = static_cast<std::uint8_t>(z);
    const auto w = static_cast<std::uint8_t>(width);
    const auto h = static_cast<std::uint8_t>(height);

    // UV coordinates for greedy meshing (use actual face size for GL_REPEAT)
    // UVs are: (0,0), (width,0), (width,height), (0,height)
    std::uint8_t u0, v0, u1, v1, u2, v2, u3, v3;

    switch (face) {
        case FACE_NEG_X: // -X face
            x0 = bx; y0 = by;     z0 = bz;
            x1 = bx; y1 = by;     z1 = bz + w;
            x2 = bx; y2 = by + h; z2 = bz + w;
            x3 = bx; y3 = by + h; z3 = bz;
            // UV: width along Z, height along Y
            u0 = 0; v0 = 0;
            u1 = w; v1 = 0;
            u2 = w; v2 = h;
            u3 = 0; v3 = h;
            break;
        case FACE_POS_X: // +X face
            x0 = bx + 1; y0 = by;     z0 = bz + w;
            x1 = bx + 1; y1 = by;     z1 = bz;
            x2 = bx + 1; y2 = by + h; z2 = bz;
            x3 = bx + 1; y3 = by + h; z3 = bz + w;
            u0 = 0; v0 = 0;
            u1 = w; v1 = 0;
            u2 = w; v2 = h;
            u3 = 0; v3 = h;
            break;
        case FACE_NEG_Y: // -Y face (bottom)
            x0 = bx;     y0 = by; z0 = bz;
            x1 = bx + w; y1 = by; z1 = bz;
            x2 = bx + w; y2 = by; z2 = bz + h;
            x3 = bx;     y3 = by; z3 = bz + h;
            // UV: width along X, height along Z
            u0 = 0; v0 = 0;
            u1 = w; v1 = 0;
            u2 = w; v2 = h;
            u3 = 0; v3 = h;
            break;
        case FACE_POS_Y: // +Y face (top)
            x0 = bx;     y0 = by + 1; z0 = bz;
            x1 = bx + w; y1 = by + 1; z1 = bz;
            x2 = bx + w; y2 = by + 1; z2 = bz + h;
            x3 = bx;     y3 = by + 1; z3 = bz + h;
            // UV: (0,0), (W,0), (W,H), (0,H) - standard quad layout
            u0 = 0; v0 = 0;
            u1 = w; v1 = 0;
            u2 = w; v2 = h;
            u3 = 0; v3 = h;
            break;
        case FACE_NEG_Z: // -Z face
            x0 = bx + w; y0 = by;     z0 = bz;
            x1 = bx;     y1 = by;     z1 = bz;
            x2 = bx;     y2 = by + h; z2 = bz;
            x3 = bx + w; y3 = by + h; z3 = bz;
            u0 = 0; v0 = 0;
            u1 = w; v1 = 0;
            u2 = w; v2 = h;
            u3 = 0; v3 = h;
            break;
        case FACE_POS_Z: // +Z face
        default:
            x0 = bx;     y0 = by;     z0 = bz + 1;
            x1 = bx + w; y1 = by;     z1 = bz + 1;
            x2 = bx + w; y2 = by + h; z2 = bz + 1;
            x3 = bx;     y3 = by + h; z3 = bz + 1;
            u0 = 0; v0 = 0;
            u1 = w; v1 = 0;
            u2 = w; v2 = h;
            u3 = 0; v3 = h;
            break;
    }

    const auto normal = static_cast<std::uint8_t>(face);
    const std::uint8_t light = data.light;
    
    // Pack AO (lower 4 bits) and fluid_level (upper 4 bits) together
    // Fluid level 0-8 stored in bits 4-7, AO in bits 0-3
    std::uint8_t ao_packed = (data.ao & 0x0F) | ((data.fluid_level & 0x0F) << 4);

    // Create 4 vertices with proper UV coordinates for greedy meshing
    PackedVertex v0_vert(x0, y0, z0, normal, tex_layer, u0, v0, light, ao_packed);
    PackedVertex v1_vert(x1, y1, z1, normal, tex_layer, u1, v1, light, ao_packed);
    PackedVertex v2_vert(x2, y2, z2, normal, tex_layer, u2, v2, light, ao_packed);
    PackedVertex v3_vert(x3, y3, z3, normal, tex_layer, u3, v3, light, ao_packed);

    mesh.add_quad(v0_vert, v1_vert, v2_vert, v3_vert);
}

// =============================================================================
// HELPER METHODS
// =============================================================================

bool MeshGenerator::is_opaque(const Chunk& chunk, LocalCoord x, LocalCoord y, LocalCoord z) const {
    if (!coord::is_valid_local(x, y, z)) {
        return false;
    }
    return chunk.get(x, y, z).is_opaque();
}

Voxel MeshGenerator::get_voxel_or_neighbor(
    const Chunk& chunk,
    LocalCoord x, LocalCoord y, LocalCoord z,
    const NeighborAccessor& neighbor_accessor
) const {
    if (coord::is_valid_local(x, y, z)) {
        return chunk.get(x, y, z);
    }

    if (neighbor_accessor) {
        const ChunkPosition& pos = chunk.position();
        ChunkCoord world_x = coord::chunk_to_world(pos.x) + x;
        ChunkCoord world_y = coord::chunk_to_world(pos.y) + y;
        ChunkCoord world_z = coord::chunk_to_world(pos.z) + z;
        return neighbor_accessor(world_x, world_y, world_z);
    }

    return Voxel{}; // Air
}

std::uint8_t MeshGenerator::calculate_ao(
    [[maybe_unused]] const Chunk& chunk,
    [[maybe_unused]] LocalCoord x, [[maybe_unused]] LocalCoord y, [[maybe_unused]] LocalCoord z,
    [[maybe_unused]] Face face, [[maybe_unused]] std::uint8_t corner,
    [[maybe_unused]] const NeighborAccessor& neighbor_accessor
) const {
    // TODO: Implement proper AO calculation
    // For now, return 0 (no occlusion)
    return 0;
}

} // namespace voxel::client
