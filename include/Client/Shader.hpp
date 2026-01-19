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

// Chunk rendering shader (packed vertex format with texture array)
constexpr const char* CHUNK_VERTEX_SHADER = R"glsl(
#version 450 core

// Packed vertex input (8 bytes total)
// data1: x(7) | y(7) | z(7) | normal(3) | tex_layer(8)
// data2: uv_u(8) | uv_v(8) | light(8) | ao(8)
layout(location = 0) in uint data1;
layout(location = 1) in uint data2;

// Per-chunk uniforms
layout(location = 0) uniform mat4 u_ViewProjection;
layout(location = 1) uniform vec3 u_ChunkOffset;  // Chunk position relative to render origin

// Outputs to fragment shader
out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;   // UV coordinates
flat out uint v_TexLayer;  // Texture layer (no interpolation!)
out float v_Light;
out float v_AO;

void main() {
    // Unpack data1: x(7) | y(7) | z(7) | normal(3) | tex_layer(8)
    uint x = data1 & 0x7Fu;                    // bits 0-6
    uint y = (data1 >> 7u) & 0x7Fu;            // bits 7-13
    uint z = (data1 >> 14u) & 0x7Fu;           // bits 14-20
    uint normalIdx = (data1 >> 21u) & 0x7u;    // bits 21-23
    uint texLayer = (data1 >> 24u) & 0xFFu;    // bits 24-31

    // Unpack data2: uv_u(8) | uv_v(8) | light(8) | ao_packed(8)
    // ao_packed: lower 4 bits = AO, upper 4 bits = fluid_level (0-8)
    uint uvU = data2 & 0xFFu;                  // bits 0-7
    uint uvV = (data2 >> 8u) & 0xFFu;          // bits 8-15
    uint light = (data2 >> 16u) & 0xFFu;       // bits 16-23
    uint aoPacked = (data2 >> 24u) & 0xFFu;    // bits 24-31
    uint ao = aoPacked & 0x0Fu;                // lower 4 bits: AO
    uint fluidLevel = (aoPacked >> 4u) & 0x0Fu; // upper 4 bits: fluid level (0-8)

    // Calculate world position (local + chunk offset)
    vec3 localPos = vec3(float(x), float(y), float(z));
    
    // Apply fluid height offset for top faces (+Y normal, index 3)
    // Fluid level 8 = full block, level 4 = half height, etc.
    if (fluidLevel > 0u && normalIdx == 3u) {
        // Lower the top face based on fluid level
        // fluidLevel 8 = 0.875 height (7/8), level 4 = 0.5 height, etc.
        float fluidHeight = float(fluidLevel) / 8.0;
        // Offset: full block = 0 offset, level 4 = -0.5 offset
        localPos.y -= (1.0 - fluidHeight * 0.875);
    }
    
    vec3 worldPos = localPos + u_ChunkOffset;

    // Transform to clip space
    gl_Position = u_ViewProjection * vec4(worldPos, 1.0);

    // Decode normal from 3-bit index
    const vec3 NORMALS[6] = vec3[6](
        vec3(-1.0, 0.0, 0.0),  // 0: -X
        vec3( 1.0, 0.0, 0.0),  // 1: +X
        vec3( 0.0,-1.0, 0.0),  // 2: -Y
        vec3( 0.0, 1.0, 0.0),  // 3: +Y
        vec3( 0.0, 0.0,-1.0),  // 4: -Z
        vec3( 0.0, 0.0, 1.0)   // 5: +Z
    );

    // Pass to fragment shader
    v_Position = worldPos;
    v_Normal = NORMALS[min(normalIdx, 5u)];
    
    // UV coordinates for greedy meshing (can be > 1.0 for GL_REPEAT)
    // If uvU and uvV are 0, use default 1x1 (corner indices)
    float u = (uvU == 0u) ? float(gl_VertexID % 2) : float(uvU);
    float v = (uvV == 0u) ? float((gl_VertexID / 2) % 2) : float(uvV);
    v_TexCoord = vec2(u, v);
    v_TexLayer = texLayer;  // Flat - no interpolation
    
    v_Light = float(light) / 255.0;
    v_AO = float(ao) / 15.0;  // AO is now 4-bit (0-15)
}
)glsl";

constexpr const char* CHUNK_FRAGMENT_SHADER = R"glsl(
#version 450 core

// Inputs from vertex shader
in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;  // UV coordinates
flat in uint v_TexLayer;  // Texture layer (no interpolation!)
in float v_Light;
in float v_AO;

// Output color
out vec4 FragColor;

// Texture array sampler
uniform sampler2DArray u_TextureArray;

// Block tinting data (sent as uniform array)
// Each vec4: (r, g, b, a) normalized 0-1
// Index by texture layer for simplicity
uniform vec4 u_BlockTints[256];

void main() {
    // Sample from texture array using UV and flat layer index
    vec3 texCoord = vec3(v_TexCoord.xy, float(v_TexLayer));
    vec4 texColor = texture(u_TextureArray, texCoord);
    
    // Discard fully transparent pixels
    if (texColor.a < 0.1) {
        discard;
    }
    
    // Get texture layer for tint lookup
    int layer = int(v_TexLayer);
    
    // Apply grayscale tinting for specific textures that are grayscale
    // grass_block_top = layer 5, oak_leaves = layer 8, water = layer 14
    // These textures need biome-based coloring
    vec4 tint = u_BlockTints[layer];
    if (layer == 5 || layer == 8 || layer == 14) {
        texColor.rgb *= tint.rgb;
    }

    // ==========================================================================
    // DIRECTIONAL LIGHTING SYSTEM
    // ==========================================================================
    
    // Sun direction - from upper-right-front
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    
    // Ambient light constant (never pitch black)
    float ambient = 0.4;
    
    // Diffuse lighting: ambient + max(0.0, dot(normal, lightDir)) * 0.6
    float diffuse = max(0.0, dot(v_Normal, lightDir));
    float lighting = ambient + diffuse * 0.6;

    // Apply ambient occlusion (v_AO is 0-1 where higher = more occlusion)
    float aoFactor = 1.0 - v_AO * 0.3;

    // Apply light level from voxel data (sun + torch)
    float lightFactor = max(v_Light, 0.2);  // Minimum light to see something

    // Final color with proper lighting
    vec3 finalColor = texColor.rgb * lighting * aoFactor * lightFactor;
    
    // Clamp to prevent over-bright
    finalColor = clamp(finalColor, 0.0, 1.0);
    
    FragColor = vec4(finalColor, texColor.a);
}
)glsl";

// =============================================================================
// BLOCK HIGHLIGHT SHADER (wireframe box)
// =============================================================================
constexpr const char* HIGHLIGHT_VERTEX_SHADER = R"glsl(
#version 450 core

layout(location = 0) in vec3 a_Position;

layout(location = 0) uniform mat4 u_ViewProjection;
layout(location = 1) uniform vec3 u_BlockPosition;  // Block world position

void main() {
    // Scale slightly larger than 1x1x1 to avoid z-fighting
    vec3 worldPos = a_Position * 1.002 + u_BlockPosition;
    gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
}
)glsl";

constexpr const char* HIGHLIGHT_FRAGMENT_SHADER = R"glsl(
#version 450 core

out vec4 FragColor;

void main() {
    // Black outline
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

} // namespace shaders

} // namespace voxel::client
