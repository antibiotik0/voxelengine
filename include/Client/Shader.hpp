// =============================================================================
// VOXEL ENGINE - SHADER MANAGEMENT
// OpenGL 4.5 Shader compilation and uniform handling
// =============================================================================
#pragma once

#include "Camera.hpp"

#include <string>
#include <string_view>
#include <cstdint>
#include <unordered_map>

namespace voxel::client {

// =============================================================================
// SHADER PROGRAM
// =============================================================================
class Shader {
public:
    Shader();
    ~Shader();

    // Non-copyable, movable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // =============================================================================
    // COMPILATION
    // =============================================================================

    // Compile from source strings
    bool compile(std::string_view vertex_source, std::string_view fragment_source);

    // Compile from file paths
    bool compile_from_files(const std::string& vertex_path, const std::string& fragment_path);

    // Get compilation error
    [[nodiscard]] const std::string& error() const noexcept { return m_error; }

    // =============================================================================
    // USAGE
    // =============================================================================

    void bind() const;
    static void unbind();

    [[nodiscard]] std::uint32_t id() const noexcept { return m_program; }
    [[nodiscard]] bool is_valid() const noexcept { return m_program != 0; }

    // =============================================================================
    // UNIFORMS
    // =============================================================================

    // Get uniform location (cached)
    [[nodiscard]] std::int32_t uniform_location(const std::string& name);

    // Set uniforms
    void set_int(const std::string& name, std::int32_t value);
    void set_uint(const std::string& name, std::uint32_t value);
    void set_float(const std::string& name, float value);
    void set_vec2(const std::string& name, float x, float y);
    void set_vec3(const std::string& name, float x, float y, float z);
    void set_vec3(const std::string& name, const math::Vec3& vec);
    void set_vec4(const std::string& name, float x, float y, float z, float w);
    void set_mat4(const std::string& name, const math::Mat4& matrix);

    // Direct location setting (for performance-critical paths)
    static void set_mat4(std::int32_t location, const math::Mat4& matrix);
    static void set_vec3(std::int32_t location, float x, float y, float z);

private:
    std::uint32_t compile_shader(std::uint32_t type, std::string_view source);
    void destroy();

private:
    std::uint32_t m_program = 0;
    std::unordered_map<std::string, std::int32_t> m_uniform_cache;
    std::string m_error;
};

// =============================================================================
// BUILT-IN SHADERS
// =============================================================================
namespace shaders {

// Chunk rendering shader (packed vertex format)
constexpr const char* CHUNK_VERTEX_SHADER = R"glsl(
#version 450 core

// Packed vertex input (8 bytes total)
layout(location = 0) in uint data1;
layout(location = 1) in uint data2;

// Per-chunk uniforms
layout(location = 0) uniform mat4 u_ViewProjection;
layout(location = 1) uniform vec3 u_ChunkOffset;  // Chunk position relative to render origin

// Outputs to fragment shader
out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;
flat out uint v_VoxelID;
out float v_Light;
out float v_AO;

// Decode normal from 3-bit index
const vec3 NORMALS[6] = vec3[6](
    vec3(-1.0, 0.0, 0.0),  // NEG_X
    vec3( 1.0, 0.0, 0.0),  // POS_X
    vec3( 0.0,-1.0, 0.0),  // NEG_Y
    vec3( 0.0, 1.0, 0.0),  // POS_Y
    vec3( 0.0, 0.0,-1.0),  // NEG_Z
    vec3( 0.0, 0.0, 1.0)   // POS_Z
);

void main() {
    // Unpack data1: position (18 bits), normal (3 bits), uv_index (11 bits)
    uint pos_x = (data1 >> 0) & 0x3Fu;
    uint pos_y = (data1 >> 6) & 0x3Fu;
    uint pos_z = (data1 >> 12) & 0x3Fu;
    uint normal_idx = (data1 >> 18) & 0x07u;
    uint uv_index = (data1 >> 21) & 0x7FFu;

    // Unpack data2: voxel_id (16 bits), light (8 bits), ao (8 bits)
    uint voxel_id = (data2 >> 0) & 0xFFFFu;
    uint light = (data2 >> 16) & 0xFFu;
    uint ao = (data2 >> 24) & 0xFFu;

    // Calculate world position relative to render origin
    vec3 local_pos = vec3(float(pos_x), float(pos_y), float(pos_z));
    vec3 world_pos = local_pos + u_ChunkOffset;

    // Output position
    gl_Position = u_ViewProjection * vec4(world_pos, 1.0);

    // Pass to fragment shader
    v_Position = world_pos;
    v_Normal = NORMALS[normal_idx];
    v_TexCoord = vec2(0.0); // TODO: Calculate from uv_index
    v_VoxelID = voxel_id;
    v_Light = float(light) / 255.0;
    v_AO = float(ao) / 255.0;
}
)glsl";

constexpr const char* CHUNK_FRAGMENT_SHADER = R"glsl(
#version 450 core

// Inputs from vertex shader
in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;
flat in uint v_VoxelID;
in float v_Light;
in float v_AO;

// Output color
out vec4 FragColor;

// Simple color palette for voxel types
vec3 get_voxel_color(uint voxel_id) {
    switch (voxel_id) {
        case 1u: return vec3(0.5, 0.5, 0.5);   // STONE - gray
        case 2u: return vec3(0.55, 0.35, 0.2); // DIRT - brown
        case 3u: return vec3(0.3, 0.7, 0.2);   // GRASS - green
        case 4u: return vec3(0.2, 0.4, 0.8);   // WATER - blue
        case 5u: return vec3(0.9, 0.85, 0.6);  // SAND - tan
        case 6u: return vec3(0.6, 0.4, 0.2);   // WOOD - brown
        case 7u: return vec3(0.2, 0.5, 0.2);   // LEAVES - dark green
        case 8u: return vec3(0.8, 0.9, 1.0);   // GLASS - light blue
        case 9u: return vec3(1.0, 1.0, 0.8);   // LIGHT - yellow
        default: return vec3(1.0, 0.0, 1.0);   // Unknown - magenta
    }
}

void main() {
    vec3 base_color = get_voxel_color(v_VoxelID);

    // Simple directional lighting
    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(v_Normal, light_dir), 0.0);
    float ambient = 0.4;

    // Apply lighting and AO
    float light_factor = ambient + diffuse * 0.6;
    light_factor *= (1.0 - v_AO * 0.3);  // AO darkening
    light_factor *= (0.5 + v_Light * 0.5);  // Block light

    vec3 final_color = base_color * light_factor;

    // Apply face-based shading for visual depth
    if (abs(v_Normal.y) > 0.5) {
        final_color *= (v_Normal.y > 0.0) ? 1.0 : 0.6;  // Top bright, bottom dark
    } else if (abs(v_Normal.x) > 0.5) {
        final_color *= 0.8;  // X faces slightly darker
    } else {
        final_color *= 0.9;  // Z faces medium
    }

    FragColor = vec4(final_color, 1.0);
}
)glsl";

} // namespace shaders

} // namespace voxel::client
