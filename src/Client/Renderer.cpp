// =============================================================================
// VOXEL ENGINE - OPENGL 4.5 RENDERER IMPLEMENTATION (AZDO)
// =============================================================================

#include "Client/Renderer.hpp"
#include "Client/Logger.hpp"

#include <glad/glad.h>

#include <iostream>
#include <cstring>
#include <algorithm>

namespace voxel::client {

// =============================================================================
// PERSISTENT BUFFER
// =============================================================================

PersistentBuffer::PersistentBuffer() = default;

PersistentBuffer::~PersistentBuffer() {
    destroy();
}

PersistentBuffer::PersistentBuffer(PersistentBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_target(other.m_target)
    , m_size(other.m_size)
    , m_mapped_ptr(other.m_mapped_ptr)
    , m_sync(other.m_sync)
{
    other.m_buffer = 0;
    other.m_mapped_ptr = nullptr;
    other.m_sync = nullptr;
}

PersistentBuffer& PersistentBuffer::operator=(PersistentBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_buffer = other.m_buffer;
        m_target = other.m_target;
        m_size = other.m_size;
        m_mapped_ptr = other.m_mapped_ptr;
        m_sync = other.m_sync;
        other.m_buffer = 0;
        other.m_mapped_ptr = nullptr;
        other.m_sync = nullptr;
    }
    return *this;
}

bool PersistentBuffer::create(std::uint32_t target, std::size_t size) {
    destroy();

    m_target = target;
    m_size = size;

    // Create buffer
    glCreateBuffers(1, &m_buffer);
    if (m_buffer == 0) {
        std::cerr << "[PersistentBuffer] Failed to create buffer\n";
        return false;
    }

    // Allocate with persistent mapping flags
    GLbitfield storage_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(m_buffer, static_cast<GLsizeiptr>(size), nullptr, storage_flags);

    // Map persistently
    GLbitfield map_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    m_mapped_ptr = glMapNamedBufferRange(m_buffer, 0, static_cast<GLsizeiptr>(size), map_flags);

    if (!m_mapped_ptr) {
        std::cerr << "[PersistentBuffer] Failed to map buffer\n";
        glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
        return false;
    }

    return true;
}

void PersistentBuffer::destroy() {
    if (m_sync) {
        glDeleteSync(static_cast<GLsync>(m_sync));
        m_sync = nullptr;
    }
    if (m_buffer != 0) {
        glUnmapNamedBuffer(m_buffer);
        glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
        m_mapped_ptr = nullptr;
    }
}

void PersistentBuffer::write(std::size_t offset, const void* data, std::size_t size) {
    if (m_mapped_ptr && offset + size <= m_size) {
        std::memcpy(static_cast<char*>(m_mapped_ptr) + offset, data, size);
    }
}

void PersistentBuffer::sync() {
    // Wait for any pending operations
    if (m_sync) {
        GLenum wait_result = glClientWaitSync(static_cast<GLsync>(m_sync), GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000); // 1 second timeout
        if (wait_result == GL_WAIT_FAILED) {
            std::cerr << "[PersistentBuffer] Sync wait failed\n";
        }
        glDeleteSync(static_cast<GLsync>(m_sync));
    }
    m_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

// =============================================================================
// RENDERER
// =============================================================================

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize() {
    if (m_initialized) {
        return true;
    }

    // Compile chunk shader
    if (!m_chunk_shader.compile(shaders::CHUNK_VERTEX_SHADER, shaders::CHUNK_FRAGMENT_SHADER)) {
        std::cerr << "[Renderer] Failed to compile chunk shader: " << m_chunk_shader.error() << "\n";
        return false;
    }

    // Compile highlight shader
    if (!m_highlight_shader.compile(shaders::HIGHLIGHT_VERTEX_SHADER, shaders::HIGHLIGHT_FRAGMENT_SHADER)) {
        std::cerr << "[Renderer] Failed to compile highlight shader: " << m_highlight_shader.error() << "\n";
        return false;
    }

    // Create highlight resources
    if (!create_highlight_resources()) {
        std::cerr << "[Renderer] Failed to create highlight resources\n";
        return false;
    }

    // Create indirect draw buffer
    std::size_t indirect_buffer_size = MAX_DRAW_COMMANDS * sizeof(DrawElementsIndirectCommand);
    if (!m_indirect_buffer.create(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_size)) {
        std::cerr << "[Renderer] Failed to create indirect buffer\n";
        return false;
    }

    // Create default 1x1 white texture (fallback)
    glCreateTextures(GL_TEXTURE_2D, 1, &m_default_texture);
    if (m_default_texture == 0) {
        std::cerr << "[Renderer] Failed to create default texture\n";
        return false;
    }
    glTextureStorage2D(m_default_texture, 1, GL_RGBA8, 1, 1);
    std::uint8_t white_pixel[4] = { 255, 255, 255, 255 };
    glTextureSubImage2D(m_default_texture, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    glTextureParameteri(m_default_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_default_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    std::cout << "[Renderer] Created default 1x1 white texture\n";

    std::cout << "[Renderer] Initialized successfully\n";
    m_initialized = true;
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Destroy all chunk GPU data
    for (auto& [pos, data] : m_chunks) {
        destroy_chunk_data(data);
    }
    m_chunks.clear();

    // Destroy highlight resources
    if (m_highlight_vao != 0) {
        glDeleteVertexArrays(1, &m_highlight_vao);
        m_highlight_vao = 0;
    }
    if (m_highlight_vbo != 0) {
        glDeleteBuffers(1, &m_highlight_vbo);
        m_highlight_vbo = 0;
    }

    // Destroy default texture
    if (m_default_texture != 0) {
        glDeleteTextures(1, &m_default_texture);
        m_default_texture = 0;
    }

    m_indirect_buffer.destroy();
    m_initialized = false;
}

// =============================================================================
// FRAME
// =============================================================================

void Renderer::begin_frame() {
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);  // Sky blue
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_draw_calls = 0;
    m_meshes_rebuilt = 0;
}

void Renderer::end_frame() {
    // Sync indirect buffer if used
    if (m_draw_calls > 0) {
        m_indirect_buffer.sync();
    }
}

// =============================================================================
// CAMERA
// =============================================================================

void Renderer::set_camera(const Camera& camera) {
    m_view_matrix = camera.view_matrix();
    m_projection_matrix = camera.projection_matrix();
    m_view_projection = camera.view_projection_matrix();
    m_render_origin = camera.render_origin();
}

// =============================================================================
// CHUNK MESH MANAGEMENT
// =============================================================================

bool Renderer::upload_chunk_mesh(const ChunkPosition& pos, const ChunkMesh& mesh) {
    if (mesh.is_empty) {
        LOG("Upload", "Mesh is empty, skipping upload for chunk (", pos.x, ",", pos.y, ",", pos.z, ")");
        return false;
    }

    LOG("Upload", "Uploading mesh for chunk (", pos.x, ",", pos.y, ",", pos.z, ")");
    LOG("Upload", "  Vertices: ", mesh.vertices.size(), " Indices: ", mesh.indices.size());

    // Remove existing if present
    if (has_chunk_mesh(pos)) {
        LOG("Upload", "  Removing existing mesh");
        remove_chunk_mesh(pos);
    }

    ChunkGPUData gpu_data;
    gpu_data.position = pos;

    if (!create_chunk_vao(gpu_data, mesh)) {
        LOG("Upload", "  ERROR: Failed to create VAO");
        return false;
    }

    m_chunks[pos] = gpu_data;

    // Update stats
    m_total_vertices += gpu_data.vertex_count;
    m_total_indices += gpu_data.index_count;
    ++m_meshes_rebuilt;

    LOG("Upload", "  Upload successful: VAO=", gpu_data.vao, " VBO=", gpu_data.vbo, " EBO=", gpu_data.ebo);

    return true;
}

void Renderer::remove_chunk_mesh(const ChunkPosition& pos) {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        m_total_vertices -= it->second.vertex_count;
        m_total_indices -= it->second.index_count;
        destroy_chunk_data(it->second);
        m_chunks.erase(it);
    }
}

bool Renderer::has_chunk_mesh(const ChunkPosition& pos) const {
    return m_chunks.find(pos) != m_chunks.end();
}

bool Renderer::create_chunk_vao(ChunkGPUData& gpu_data, const ChunkMesh& mesh) {
    LOG("VAO", "Creating VAO for ", mesh.vertices.size(), " vertices, ", mesh.indices.size(), " indices");

    // Create VAO
    glCreateVertexArrays(1, &gpu_data.vao);
    if (gpu_data.vao == 0) {
        LOG("VAO", "ERROR: glCreateVertexArrays failed");
        return false;
    }
    LOG("VAO", "Created VAO: ", gpu_data.vao);

    // Create VBO for vertices
    glCreateBuffers(1, &gpu_data.vbo);
    GLsizeiptr vbo_size = static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(PackedVertex));
    glNamedBufferStorage(gpu_data.vbo, vbo_size, mesh.vertices.data(), 0);
    LOG("VAO", "Created VBO: ", gpu_data.vbo, " size=", vbo_size, " bytes");

    // Log first few vertices raw data
    for (std::size_t i = 0; i < std::min<std::size_t>(4, mesh.vertices.size()); ++i) {
        LOG("VAO", "  V", i, " data1=0x", std::hex, mesh.vertices[i].data1, " data2=0x", mesh.vertices[i].data2, std::dec);
    }

    // Create EBO for indices
    glCreateBuffers(1, &gpu_data.ebo);
    GLsizeiptr ebo_size = static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t));
    glNamedBufferStorage(gpu_data.ebo, ebo_size, mesh.indices.data(), 0);
    LOG("VAO", "Created EBO: ", gpu_data.ebo, " size=", ebo_size, " bytes");

    // Log first few indices
    LOG("VAO", "First indices: ", mesh.indices[0], ", ", mesh.indices[1], ", ", mesh.indices[2], ", ",
                                  mesh.indices[3], ", ", mesh.indices[4], ", ", mesh.indices[5]);

    // Setup VAO attributes
    // Attribute 0: data1 (uint32)
    glEnableVertexArrayAttrib(gpu_data.vao, 0);
    glVertexArrayAttribIFormat(gpu_data.vao, 0, 1, GL_UNSIGNED_INT, offsetof(PackedVertex, data1));
    glVertexArrayAttribBinding(gpu_data.vao, 0, 0);
    LOG("VAO", "Attrib 0: IFormat size=1 type=UINT offset=", offsetof(PackedVertex, data1));

    // Attribute 1: data2 (uint32)
    glEnableVertexArrayAttrib(gpu_data.vao, 1);
    glVertexArrayAttribIFormat(gpu_data.vao, 1, 1, GL_UNSIGNED_INT, offsetof(PackedVertex, data2));
    glVertexArrayAttribBinding(gpu_data.vao, 1, 0);
    LOG("VAO", "Attrib 1: IFormat size=1 type=UINT offset=", offsetof(PackedVertex, data2));

    // Bind VBO to binding point 0
    glVertexArrayVertexBuffer(gpu_data.vao, 0, gpu_data.vbo, 0, sizeof(PackedVertex));
    LOG("VAO", "Bound VBO to binding point 0, stride=", sizeof(PackedVertex));

    // Bind EBO
    glVertexArrayElementBuffer(gpu_data.vao, gpu_data.ebo);
    LOG("VAO", "Bound EBO to VAO");

    gpu_data.vertex_count = static_cast<std::uint32_t>(mesh.vertices.size());
    gpu_data.index_count = static_cast<std::uint32_t>(mesh.indices.size());
    gpu_data.valid = true;

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG("VAO", "OpenGL ERROR: ", err);
        return false;
    }

    LOG("VAO", "VAO creation successful");
    return true;
}

void Renderer::destroy_chunk_data(ChunkGPUData& data) {
    if (data.vao != 0) {
        glDeleteVertexArrays(1, &data.vao);
        data.vao = 0;
    }
    if (data.vbo != 0) {
        glDeleteBuffers(1, &data.vbo);
        data.vbo = 0;
    }
    if (data.ebo != 0) {
        glDeleteBuffers(1, &data.ebo);
        data.ebo = 0;
    }
    data.valid = false;
}

// =============================================================================
// RENDERING
// =============================================================================

void Renderer::render_chunks() {
    if (m_chunks.empty()) {
        LOG("Render", "No chunks to render");
        return;
    }

    // Bind shader
    m_chunk_shader.bind();
    
    // Debug: Check if shader is valid
    static bool first_shader_check = true;
    if (first_shader_check) {
        if (!m_chunk_shader.is_valid()) {
            LOG("Render", "ERROR: Chunk shader is not valid!");
        } else {
            LOG("Render", "Shader program ID: ", m_chunk_shader.id());
        }
        first_shader_check = false;
    }

    // Set view-projection matrix (location 0)
    Shader::set_mat4(0, m_view_projection);

    static bool first_render = true;

    // Render each chunk
    for (const auto& [pos, gpu_data] : m_chunks) {
        if (!gpu_data.valid || gpu_data.index_count == 0) {
            if (first_render) {
                LOG("Render", "Skipping invalid chunk at (", pos.x, ",", pos.y, ",", pos.z, ")");
            }
            continue;
        }

        // Calculate chunk offset relative to render origin
        float chunk_offset_x = static_cast<float>(static_cast<double>(pos.x * static_cast<std::int32_t>(CHUNK_SIZE_X)) - m_render_origin.x);
        float chunk_offset_y = static_cast<float>(static_cast<double>(pos.y * static_cast<std::int32_t>(CHUNK_SIZE_Y)) - m_render_origin.y);
        float chunk_offset_z = static_cast<float>(static_cast<double>(pos.z * static_cast<std::int32_t>(CHUNK_SIZE_Z)) - m_render_origin.z);

        if (first_render) {
            LOG("Render", "Chunk (", pos.x, ",", pos.y, ",", pos.z, "):");
            LOG("Render", "  VAO=", gpu_data.vao, " VBO=", gpu_data.vbo, " EBO=", gpu_data.ebo);
            LOG("Render", "  Vertices=", gpu_data.vertex_count, " Indices=", gpu_data.index_count);
            LOG("Render", "  Offset=(", chunk_offset_x, ",", chunk_offset_y, ",", chunk_offset_z, ")");
            LOG("Render", "  Render origin=(", m_render_origin.x, ",", m_render_origin.y, ",", m_render_origin.z, ")");
        }

        // Set chunk offset uniform (location 1)
        Shader::set_vec3(1, chunk_offset_x, chunk_offset_y, chunk_offset_z);

        // Bind VAO and draw
        glBindVertexArray(gpu_data.vao);
        
        if (first_render) {
            // Verify VAO binding
            GLint current_vao = 0;
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &current_vao);
            LOG("Render", "  Bound VAO: ", current_vao, " (expected ", gpu_data.vao, ")");
            
            // Verify EBO binding
            GLint current_ebo = 0;
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &current_ebo);
            LOG("Render", "  Bound EBO: ", current_ebo, " (expected ", gpu_data.ebo, ")");
        }

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gpu_data.index_count), GL_UNSIGNED_INT, nullptr);

        if (first_render) {
            // Check for OpenGL errors
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                LOG("Render", "  OpenGL ERROR after draw: ", err);
            } else {
                LOG("Render", "  Draw call succeeded");
            }
        }

        ++m_draw_calls;
    }

    if (first_render) {
        LOG("Render", "Total draw calls: ", m_draw_calls);
        first_render = false;
    }

    glBindVertexArray(0);
    Shader::unbind();
}

void Renderer::render_chunk(const ChunkPosition& pos) {
    auto it = m_chunks.find(pos);
    if (it == m_chunks.end() || !it->second.valid) {
        return;
    }

    const auto& gpu_data = it->second;

    m_chunk_shader.bind();

    // Set view-projection matrix
    Shader::set_mat4(0, m_view_projection);

    // Calculate chunk offset
    float chunk_offset_x = static_cast<float>(static_cast<double>(pos.x * static_cast<std::int32_t>(CHUNK_SIZE_X)) - m_render_origin.x);
    float chunk_offset_y = static_cast<float>(static_cast<double>(pos.y * static_cast<std::int32_t>(CHUNK_SIZE_Y)) - m_render_origin.y);
    float chunk_offset_z = static_cast<float>(static_cast<double>(pos.z * static_cast<std::int32_t>(CHUNK_SIZE_Z)) - m_render_origin.z);

    Shader::set_vec3(1, chunk_offset_x, chunk_offset_y, chunk_offset_z);

    glBindVertexArray(gpu_data.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gpu_data.index_count), GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    Shader::unbind();

    ++m_draw_calls;
}

void Renderer::build_draw_batch(DrawBatch& batch, const math::Vec3& /*camera_pos*/) {
    batch.commands.clear();
    batch.chunk_offsets.clear();
    batch.commands.reserve(m_chunks.size());
    batch.chunk_offsets.reserve(m_chunks.size());

    std::uint32_t base_vertex = 0;
    std::uint32_t base_index = 0;

    for (const auto& [pos, gpu_data] : m_chunks) {
        if (!gpu_data.valid || gpu_data.index_count == 0) {
            continue;
        }

        // TODO: Frustum culling here

        DrawElementsIndirectCommand cmd;
        cmd.count = gpu_data.index_count;
        cmd.instance_count = 1;
        cmd.first_index = base_index;
        cmd.base_vertex = base_vertex;
        cmd.base_instance = static_cast<std::uint32_t>(batch.commands.size());

        batch.commands.push_back(cmd);

        // Calculate chunk offset
        math::Vec3 offset;
        offset.x = static_cast<float>(static_cast<double>(pos.x * static_cast<std::int32_t>(CHUNK_SIZE_X)) - m_render_origin.x);
        offset.y = static_cast<float>(static_cast<double>(pos.y * static_cast<std::int32_t>(CHUNK_SIZE_Y)) - m_render_origin.y);
        offset.z = static_cast<float>(static_cast<double>(pos.z * static_cast<std::int32_t>(CHUNK_SIZE_Z)) - m_render_origin.z);
        batch.chunk_offsets.push_back(offset);

        // Note: In a true AZDO setup, we'd pack all chunk meshes into large buffers
        // For now, we're using per-chunk VAOs for simplicity
    }
}

// =============================================================================
// BLOCK HIGHLIGHT
// =============================================================================

bool Renderer::create_highlight_resources() {
    // Wireframe cube vertices (12 edges = 24 line vertices)
    // Each edge is a line from one corner to another
    static const float cube_lines[] = {
        // Bottom face edges
        0.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,  // edge 0
        1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 1.0f,  // edge 1
        1.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,  // edge 2
        0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f,  // edge 3
        // Top face edges
        0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 0.0f,  // edge 4
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  // edge 5
        1.0f, 1.0f, 1.0f,   0.0f, 1.0f, 1.0f,  // edge 6
        0.0f, 1.0f, 1.0f,   0.0f, 1.0f, 0.0f,  // edge 7
        // Vertical edges
        0.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f,  // edge 8
        1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 0.0f,  // edge 9
        1.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  // edge 10
        0.0f, 0.0f, 1.0f,   0.0f, 1.0f, 1.0f,  // edge 11
    };

    // Create VAO
    glCreateVertexArrays(1, &m_highlight_vao);
    if (m_highlight_vao == 0) {
        return false;
    }

    // Create VBO
    glCreateBuffers(1, &m_highlight_vbo);
    glNamedBufferStorage(m_highlight_vbo, sizeof(cube_lines), cube_lines, 0);

    // Setup vertex attribute (position only)
    glEnableVertexArrayAttrib(m_highlight_vao, 0);
    glVertexArrayAttribFormat(m_highlight_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_highlight_vao, 0, 0);

    // Bind VBO
    glVertexArrayVertexBuffer(m_highlight_vao, 0, m_highlight_vbo, 0, 3 * sizeof(float));

    return true;
}

void Renderer::render_block_highlight(std::int32_t block_x, std::int32_t block_y, std::int32_t block_z) {
    if (m_highlight_vao == 0) return;

    // Calculate block position relative to render origin
    float rel_x = static_cast<float>(static_cast<double>(block_x) - m_render_origin.x);
    float rel_y = static_cast<float>(static_cast<double>(block_y) - m_render_origin.y);
    float rel_z = static_cast<float>(static_cast<double>(block_z) - m_render_origin.z);

    // Bind highlight shader
    m_highlight_shader.bind();
    
    // Set uniforms
    Shader::set_mat4(0, m_view_projection);
    Shader::set_vec3(1, rel_x, rel_y, rel_z);

    // Disable depth test slightly to prevent z-fighting
    glDisable(GL_CULL_FACE);

    // Draw wireframe cube
    glBindVertexArray(m_highlight_vao);
    glDrawArrays(GL_LINES, 0, 24);  // 12 edges * 2 vertices each
    glBindVertexArray(0);

    // Restore state
    glEnable(GL_CULL_FACE);
    
    Shader::unbind();
}

void Renderer::set_cull_face(bool enabled) {
    if (enabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

} // namespace voxel::client
