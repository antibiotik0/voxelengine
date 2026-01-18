// =============================================================================
// VOXEL ENGINE - OPENGL 4.5 RENDERER (AZDO)
// Approaching Zero Driver Overhead - Persistent Mapped Buffers + Indirect Draw
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include "Client/Camera.hpp"
#include "Client/Shader.hpp"
#include "Client/ChunkMesh.hpp"

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace voxel::client {

// =============================================================================
// GPU BUFFER - PERSISTENT MAPPED
// =============================================================================
class PersistentBuffer {
public:
    PersistentBuffer();
    ~PersistentBuffer();

    PersistentBuffer(const PersistentBuffer&) = delete;
    PersistentBuffer& operator=(const PersistentBuffer&) = delete;
    PersistentBuffer(PersistentBuffer&& other) noexcept;
    PersistentBuffer& operator=(PersistentBuffer&& other) noexcept;

    // Create persistent buffer with given target (GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, etc.)
    bool create(std::uint32_t target, std::size_t size);
    void destroy();

    [[nodiscard]] bool is_valid() const noexcept { return m_buffer != 0; }
    [[nodiscard]] std::uint32_t id() const noexcept { return m_buffer; }
    [[nodiscard]] std::size_t size() const noexcept { return m_size; }
    [[nodiscard]] void* data() noexcept { return m_mapped_ptr; }
    [[nodiscard]] const void* data() const noexcept { return m_mapped_ptr; }

    // Write data to buffer (memcpy to persistent mapping)
    void write(std::size_t offset, const void* data, std::size_t size);

    // Explicit sync (call before draw if needed)
    void sync();

private:
    std::uint32_t m_buffer = 0;
    std::uint32_t m_target = 0;
    std::size_t m_size = 0;
    void* m_mapped_ptr = nullptr;
    void* m_sync = nullptr;  // GLsync
};

// =============================================================================
// CHUNK GPU DATA
// =============================================================================
struct ChunkGPUData {
    std::uint32_t vao = 0;
    std::uint32_t vbo = 0;
    std::uint32_t ebo = 0;

    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;

    ChunkPosition position;
    bool valid = false;
};

// =============================================================================
// DRAW COMMAND BATCH
// =============================================================================
struct DrawBatch {
    std::vector<DrawElementsIndirectCommand> commands;
    std::vector<math::Vec3> chunk_offsets;  // For uniform buffer
};

// =============================================================================
// RENDERER
// =============================================================================
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // =============================================================================
    // INITIALIZATION
    // =============================================================================

    bool initialize();
    void shutdown();

    [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }

    // =============================================================================
    // FRAME
    // =============================================================================

    // Begin new frame (clear buffers, reset state)
    void begin_frame();

    // End frame (finalize draws)
    void end_frame();

    // =============================================================================
    // CAMERA
    // =============================================================================

    void set_camera(const Camera& camera);

    // =============================================================================
    // CHUNK MESH MANAGEMENT
    // =============================================================================

    // Upload a chunk mesh to GPU (returns true on success)
    bool upload_chunk_mesh(const ChunkPosition& pos, const ChunkMesh& mesh);

    // Remove a chunk mesh from GPU
    void remove_chunk_mesh(const ChunkPosition& pos);

    // Check if chunk is uploaded
    [[nodiscard]] bool has_chunk_mesh(const ChunkPosition& pos) const;

    // =============================================================================
    // RENDERING
    // =============================================================================

    // Render all visible chunks
    void render_chunks();

    // Render single chunk (debug)
    void render_chunk(const ChunkPosition& pos);

    // =============================================================================
    // STATISTICS
    // =============================================================================

    [[nodiscard]] std::size_t uploaded_chunk_count() const noexcept { return m_chunks.size(); }
    [[nodiscard]] std::size_t total_vertices() const noexcept { return m_total_vertices; }
    [[nodiscard]] std::size_t total_indices() const noexcept { return m_total_indices; }
    [[nodiscard]] std::size_t draw_calls_last_frame() const noexcept { return m_draw_calls; }

    // =============================================================================
    // DEBUG / VISUALIZATION
    // =============================================================================

    void set_wireframe(bool enabled);
    void set_cull_face(bool enabled);

private:
    bool create_chunk_vao(ChunkGPUData& gpu_data, const ChunkMesh& mesh);
    void destroy_chunk_data(ChunkGPUData& data);

    // Build indirect draw commands for visible chunks
    void build_draw_batch(DrawBatch& batch, const math::Vec3& camera_pos);

private:
    bool m_initialized = false;

    // Shaders
    Shader m_chunk_shader;

    // Camera state
    math::Mat4 m_view_matrix;
    math::Mat4 m_projection_matrix;
    math::Mat4 m_view_projection;
    WorldPosition m_render_origin;  // Origin for camera-relative rendering

    // Chunk meshes on GPU
    std::unordered_map<ChunkPosition, ChunkGPUData> m_chunks;

    // Indirect draw buffer
    PersistentBuffer m_indirect_buffer;
    static constexpr std::size_t MAX_DRAW_COMMANDS = 4096;

    // Statistics
    std::size_t m_total_vertices = 0;
    std::size_t m_total_indices = 0;
    std::size_t m_draw_calls = 0;

    // Render state
    bool m_wireframe = false;
};

} // namespace voxel::client
