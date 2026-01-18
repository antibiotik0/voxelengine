// =============================================================================
// VOXEL ENGINE - SHADER MANAGEMENT IMPLEMENTATION
// =============================================================================

#include "Client/Shader.hpp"

#include <glad/glad.h>

#include <fstream>
#include <sstream>
#include <iostream>

namespace voxel::client {

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================

Shader::Shader() = default;

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program)
    , m_uniform_cache(std::move(other.m_uniform_cache))
    , m_error(std::move(other.m_error))
{
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        m_program = other.m_program;
        m_uniform_cache = std::move(other.m_uniform_cache);
        m_error = std::move(other.m_error);
        other.m_program = 0;
    }
    return *this;
}

void Shader::destroy() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniform_cache.clear();
}

// =============================================================================
// COMPILATION
// =============================================================================

bool Shader::compile(std::string_view vertex_source, std::string_view fragment_source) {
    destroy();
    m_error.clear();

    // Compile vertex shader
    std::uint32_t vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (vertex_shader == 0) {
        return false;
    }

    // Compile fragment shader
    std::uint32_t fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return false;
    }

    // Create program and link
    m_program = glCreateProgram();
    glAttachShader(m_program, vertex_shader);
    glAttachShader(m_program, fragment_shader);
    glLinkProgram(m_program);

    // Check link status
    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &log_length);
        m_error.resize(static_cast<std::size_t>(log_length));
        glGetProgramInfoLog(m_program, log_length, nullptr, m_error.data());

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    // Clean up shaders (they're linked into the program now)
    glDetachShader(m_program, vertex_shader);
    glDetachShader(m_program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return true;
}

bool Shader::compile_from_files(const std::string& vertex_path, const std::string& fragment_path) {
    // Read vertex shader
    std::ifstream vertex_file(vertex_path);
    if (!vertex_file.is_open()) {
        m_error = "Failed to open vertex shader file: " + vertex_path;
        return false;
    }
    std::stringstream vertex_stream;
    vertex_stream << vertex_file.rdbuf();
    std::string vertex_source = vertex_stream.str();

    // Read fragment shader
    std::ifstream fragment_file(fragment_path);
    if (!fragment_file.is_open()) {
        m_error = "Failed to open fragment shader file: " + fragment_path;
        return false;
    }
    std::stringstream fragment_stream;
    fragment_stream << fragment_file.rdbuf();
    std::string fragment_source = fragment_stream.str();

    return compile(vertex_source, fragment_source);
}

std::uint32_t Shader::compile_shader(std::uint32_t type, std::string_view source) {
    std::uint32_t shader = glCreateShader(type);

    const char* source_ptr = source.data();
    auto length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &source_ptr, &length);
    glCompileShader(shader);

    // Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        m_error.resize(static_cast<std::size_t>(log_length));
        glGetShaderInfoLog(shader, log_length, nullptr, m_error.data());

        const char* type_str = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        m_error = std::string(type_str) + " SHADER ERROR: " + m_error;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// =============================================================================
// USAGE
// =============================================================================

void Shader::bind() const {
    glUseProgram(m_program);
}

void Shader::unbind() {
    glUseProgram(0);
}

// =============================================================================
// UNIFORMS
// =============================================================================

std::int32_t Shader::uniform_location(const std::string& name) {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }

    std::int32_t location = glGetUniformLocation(m_program, name.c_str());
    m_uniform_cache[name] = location;
    return location;
}

void Shader::set_int(const std::string& name, std::int32_t value) {
    glUniform1i(uniform_location(name), value);
}

void Shader::set_uint(const std::string& name, std::uint32_t value) {
    glUniform1ui(uniform_location(name), value);
}

void Shader::set_float(const std::string& name, float value) {
    glUniform1f(uniform_location(name), value);
}

void Shader::set_vec2(const std::string& name, float x, float y) {
    glUniform2f(uniform_location(name), x, y);
}

void Shader::set_vec3(const std::string& name, float x, float y, float z) {
    glUniform3f(uniform_location(name), x, y, z);
}

void Shader::set_vec3(const std::string& name, const math::Vec3& vec) {
    glUniform3f(uniform_location(name), vec.x, vec.y, vec.z);
}

void Shader::set_vec4(const std::string& name, float x, float y, float z, float w) {
    glUniform4f(uniform_location(name), x, y, z, w);
}

void Shader::set_mat4(const std::string& name, const math::Mat4& matrix) {
    glUniformMatrix4fv(uniform_location(name), 1, GL_FALSE, matrix.data.data());
}

void Shader::set_mat4(std::int32_t location, const math::Mat4& matrix) {
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix.data.data());
}

void Shader::set_vec3(std::int32_t location, float x, float y, float z) {
    glUniform3f(location, x, y, z);
}

} // namespace voxel::client
