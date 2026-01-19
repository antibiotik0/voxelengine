// =============================================================================
// VOXEL ENGINE - MESH TASK QUEUE
// Background thread pool for chunk mesh generation
// Uses dirty chunk system for efficient updates
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Shared/ThreadPool.hpp"
#include "Client/MeshGenerator.hpp"
#include "Client/ChunkMesh.hpp"

#include <queue>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

namespace voxel::client {

// =============================================================================
// MESH TASK RESULT
// =============================================================================
struct MeshTaskResult {
    ChunkPosition position;
    ChunkMesh mesh;
    bool success;
};

// =============================================================================
// MESH TASK QUEUE
// Manages background mesh generation with priority and deduplication
// =============================================================================
class MeshTaskQueue {
public:
    // Voxel accessor callback type
    using VoxelAccessor = std::function<Voxel(ChunkCoord, ChunkCoord, ChunkCoord)>;
    using ChunkAccessor = std::function<const Chunk*(ChunkCoord, ChunkCoord, ChunkCoord)>;
    
    explicit MeshTaskQueue(std::size_t num_threads = 0)
        : m_pool(num_threads > 0 ? num_threads : 4)
        , m_pending_count(0)
        , m_completed_count(0)
    {}
    
    ~MeshTaskQueue() {
        shutdown();
    }
    
    // =============================================================================
    // TASK SUBMISSION
    // =============================================================================
    
    // Queue a chunk for mesh regeneration
    // Deduplicates: if chunk is already queued, skip
    void queue_remesh(
        ChunkPosition pos,
        const Chunk& chunk,
        VoxelAccessor voxel_accessor
    ) {
        {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            
            // Skip if already queued
            if (m_queued_positions.count(pos) > 0) {
                return;
            }
            m_queued_positions.insert(pos);
        }
        
        // Capture chunk data (copy voxels for thread safety)
        auto chunk_data = std::make_shared<std::vector<Voxel>>();
        chunk_data->reserve(CHUNK_VOLUME);
        for (std::uint32_t i = 0; i < CHUNK_VOLUME; ++i) {
            chunk_data->push_back(chunk.get_voxel_by_index(i));
        }
        
        ChunkPosition chunk_pos = chunk.position();
        
        m_pending_count++;
        
        // Submit to thread pool
        m_pool.submit_detached([this, pos, chunk_data, chunk_pos, voxel_accessor]() {
            generate_mesh(pos, *chunk_data, chunk_pos, voxel_accessor);
        });
    }
    
    // Queue multiple chunks for remesh (batch operation)
    void queue_remesh_batch(
        const std::vector<ChunkPosition>& positions,
        ChunkAccessor chunk_accessor,
        VoxelAccessor voxel_accessor
    ) {
        for (const auto& pos : positions) {
            const Chunk* chunk = chunk_accessor(pos.x, pos.y, pos.z);
            if (chunk) {
                queue_remesh(pos, *chunk, voxel_accessor);
            }
        }
    }
    
    // =============================================================================
    // RESULT RETRIEVAL
    // =============================================================================
    
    // Get completed mesh results (call from main thread)
    // Returns up to max_results completed meshes
    std::vector<MeshTaskResult> get_completed(std::size_t max_results = 16) {
        std::vector<MeshTaskResult> results;
        results.reserve(max_results);
        
        std::lock_guard<std::mutex> lock(m_results_mutex);
        
        while (!m_completed_results.empty() && results.size() < max_results) {
            results.push_back(std::move(m_completed_results.front()));
            m_completed_results.pop();
        }
        
        return results;
    }
    
    // Check if there are completed results waiting
    [[nodiscard]] bool has_completed() const {
        std::lock_guard<std::mutex> lock(m_results_mutex);
        return !m_completed_results.empty();
    }
    
    // =============================================================================
    // STATUS
    // =============================================================================
    
    [[nodiscard]] std::size_t pending_count() const noexcept {
        return m_pending_count.load();
    }
    
    [[nodiscard]] std::size_t completed_count() const noexcept {
        return m_completed_count.load();
    }
    
    [[nodiscard]] std::size_t worker_count() const noexcept {
        return m_pool.size();
    }
    
    // Wait for all pending tasks to complete
    void wait_idle() {
        m_pool.wait_idle();
    }
    
    // Shutdown the queue
    void shutdown() {
        m_pool.shutdown();
    }
    
private:
    void generate_mesh(
        ChunkPosition pos,
        const std::vector<Voxel>& chunk_data,
        ChunkPosition chunk_pos,
        VoxelAccessor voxel_accessor
    ) {
        // Create a temporary chunk wrapper for the mesh generator
        // This is a bit of a hack but avoids copying the entire Chunk class
        struct TempChunk {
            const std::vector<Voxel>& data;
            ChunkPosition pos;
            
            [[nodiscard]] Voxel get_voxel(LocalCoord x, LocalCoord y, LocalCoord z) const {
                if (x < 0 || x >= 64 || y < 0 || y >= 64 || z < 0 || z >= 64) {
                    return Voxel(VoxelType::AIR);
                }
                VoxelIndex idx = coord::to_index(x, y, z);
                return data[idx];
            }
            
            [[nodiscard]] const ChunkPosition& position() const { return pos; }
        };
        
        TempChunk temp_chunk{chunk_data, chunk_pos};
        
        // Create mesh generator (thread-local to avoid contention)
        thread_local MeshGenerator mesh_gen;
        
        ChunkMesh mesh;
        
        // Generate mesh with neighbor accessor
        // The neighbor accessor calls back to the main world for cross-chunk data
        auto neighbor_accessor = [&voxel_accessor, &temp_chunk](ChunkCoord wx, ChunkCoord wy, ChunkCoord wz) -> Voxel {
            // Convert to chunk-local space
            ChunkCoord chunk_origin_x = temp_chunk.pos.x << 6;
            ChunkCoord chunk_origin_y = temp_chunk.pos.y << 6;
            ChunkCoord chunk_origin_z = temp_chunk.pos.z << 6;
            
            // If within this chunk, use local data
            LocalCoord lx = static_cast<LocalCoord>(wx - chunk_origin_x);
            LocalCoord ly = static_cast<LocalCoord>(wy - chunk_origin_y);
            LocalCoord lz = static_cast<LocalCoord>(wz - chunk_origin_z);
            
            if (lx >= 0 && lx < 64 && ly >= 0 && ly < 64 && lz >= 0 && lz < 64) {
                return temp_chunk.get_voxel(lx, ly, lz);
            }
            
            // Otherwise query world (this is the cross-chunk case)
            return voxel_accessor(wx, wy, wz);
        };
        
        // Generate using a simple face-by-face approach for the copied data
        // This is a simplified version since we can't use the full Chunk interface
        generate_mesh_from_data(chunk_data, chunk_pos, mesh, neighbor_accessor);
        
        // Store result
        {
            std::lock_guard<std::mutex> lock(m_results_mutex);
            m_completed_results.push(MeshTaskResult{pos, std::move(mesh), true});
        }
        
        // Update counters
        m_pending_count--;
        m_completed_count++;
        
        // Remove from queued set
        {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            m_queued_positions.erase(pos);
        }
    }
    
    void generate_mesh_from_data(
        const std::vector<Voxel>& data,
        ChunkPosition chunk_pos,
        ChunkMesh& out_mesh,
        const NeighborAccessor& neighbor_accessor
    ) {
        // Simple naive mesh generation (per-face)
        // For production, use greedy meshing
        
        out_mesh.vertices.clear();
        out_mesh.indices.clear();
        out_mesh.is_empty = true;
        
        ChunkCoord origin_x = chunk_pos.x << 6;
        ChunkCoord origin_y = chunk_pos.y << 6;
        ChunkCoord origin_z = chunk_pos.z << 6;
        
        // Face normal directions
        static constexpr int DX[] = {-1, 1, 0, 0, 0, 0};
        static constexpr int DY[] = {0, 0, -1, 1, 0, 0};
        static constexpr int DZ[] = {0, 0, 0, 0, -1, 1};
        
        std::uint32_t vertex_count = 0;
        
        for (LocalCoord x = 0; x < 64; ++x) {
            for (LocalCoord y = 0; y < 64; ++y) {
                for (LocalCoord z = 0; z < 64; ++z) {
                    VoxelIndex idx = coord::to_index(x, y, z);
                    const Voxel& voxel = data[idx];
                    
                    if (voxel.is_air()) continue;
                    
                    // Check each face
                    for (int face = 0; face < 6; ++face) {
                        ChunkCoord nx = origin_x + x + DX[face];
                        ChunkCoord ny = origin_y + y + DY[face];
                        ChunkCoord nz = origin_z + z + DZ[face];
                        
                        Voxel neighbor = neighbor_accessor(nx, ny, nz);
                        
                        // Skip if neighbor is solid
                        if (!neighbor.is_air()) continue;
                        
                        // Add face vertices
                        add_face(out_mesh, x, y, z, face, voxel.type_id(), vertex_count);
                        out_mesh.is_empty = false;
                    }
                }
            }
        }
    }
    
    void add_face(ChunkMesh& mesh, LocalCoord x, LocalCoord y, LocalCoord z, 
                  int face, std::uint16_t block_type, std::uint32_t& vertex_count) {
        // Face vertices in local chunk space
        // Simplified - just create 4 vertices per face
        
        static constexpr float FACE_VERTICES[6][4][3] = {
            // -X face
            {{0,0,0}, {0,0,1}, {0,1,1}, {0,1,0}},
            // +X face
            {{1,0,1}, {1,0,0}, {1,1,0}, {1,1,1}},
            // -Y face
            {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}},
            // +Y face
            {{0,1,1}, {1,1,1}, {1,1,0}, {0,1,0}},
            // -Z face
            {{1,0,0}, {0,0,0}, {0,1,0}, {1,1,0}},
            // +Z face
            {{0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}}
        };
        
        static constexpr std::uint8_t FACE_NORMALS[6] = {0, 1, 2, 3, 4, 5};
        
        for (int i = 0; i < 4; ++i) {
            PackedVertex v;
            v.pack_position(
                static_cast<std::uint8_t>(x) + static_cast<std::uint8_t>(FACE_VERTICES[face][i][0]),
                static_cast<std::uint8_t>(y) + static_cast<std::uint8_t>(FACE_VERTICES[face][i][1]),
                static_cast<std::uint8_t>(z) + static_cast<std::uint8_t>(FACE_VERTICES[face][i][2])
            );
            v.pack_texcoord(
                (i == 1 || i == 2) ? 1 : 0,
                (i == 2 || i == 3) ? 1 : 0
            );
            v.pack_normal(FACE_NORMALS[face]);
            v.pack_ao(3); // No AO for now
            v.pack_block_type(block_type);
            
            mesh.vertices.push_back(v);
        }
        
        // Add indices (two triangles per face)
        mesh.indices.push_back(vertex_count + 0);
        mesh.indices.push_back(vertex_count + 1);
        mesh.indices.push_back(vertex_count + 2);
        mesh.indices.push_back(vertex_count + 0);
        mesh.indices.push_back(vertex_count + 2);
        mesh.indices.push_back(vertex_count + 3);
        
        vertex_count += 4;
    }
    
    ThreadPool m_pool;
    
    // Pending tasks tracking
    std::mutex m_pending_mutex;
    std::unordered_set<ChunkPosition> m_queued_positions;
    std::atomic<std::size_t> m_pending_count;
    
    // Completed results
    mutable std::mutex m_results_mutex;
    std::queue<MeshTaskResult> m_completed_results;
    std::atomic<std::size_t> m_completed_count;
};

} // namespace voxel::client
