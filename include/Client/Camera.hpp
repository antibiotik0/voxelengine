// =============================================================================
// VOXEL ENGINE - CAMERA SYSTEM
// First-person camera with origin shifting for jitter-free rendering
// at extreme world coordinates (±10,000,000 units)
// =============================================================================
#pragma once

#include "Shared/Types.hpp"

#include <cmath>
#include <array>

namespace voxel::client {

// =============================================================================
// MATH UTILITIES
// =============================================================================
namespace math {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

// 3D Vector (float)
struct Vec3 {
    float x, y, z;

    constexpr Vec3() noexcept : x(0), y(0), z(0) {}
    constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    constexpr Vec3 operator-(const Vec3& other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
    constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
    constexpr Vec3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    [[nodiscard]] Vec3 normalized() const noexcept {
        const float len = length();
        if (len > 0.0001f) {
            return {x / len, y / len, z / len};
        }
        return {0, 0, 0};
    }

    [[nodiscard]] static constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    [[nodiscard]] static constexpr float dot(const Vec3& a, const Vec3& b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

// 4x4 Matrix (column-major for OpenGL)
struct Mat4 {
    std::array<float, 16> data;

    Mat4() noexcept { identity(); }

    void identity() noexcept {
        data = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
    }

    [[nodiscard]] float& operator()(int row, int col) noexcept {
        return data[col * 4 + row];
    }
    [[nodiscard]] float operator()(int row, int col) const noexcept {
        return data[col * 4 + row];
    }

    [[nodiscard]] const float* ptr() const noexcept { return data.data(); }

    // Matrix multiplication
    [[nodiscard]] Mat4 operator*(const Mat4& other) const noexcept {
        Mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += (*this)(row, k) * other(k, col);
                }
                result(row, col) = sum;
            }
        }
        return result;
    }

    // Static factory methods
    [[nodiscard]] static Mat4 perspective(float fov_radians, float aspect, float near, float far) noexcept {
        Mat4 m;
        const float tan_half_fov = std::tan(fov_radians * 0.5f);

        m.data.fill(0.0f);
        m(0, 0) = 1.0f / (aspect * tan_half_fov);
        m(1, 1) = 1.0f / tan_half_fov;
        m(2, 2) = -(far + near) / (far - near);
        m(2, 3) = -1.0f;
        m(3, 2) = -(2.0f * far * near) / (far - near);

        return m;
    }

    [[nodiscard]] static Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) noexcept {
        const Vec3 f = (center - eye).normalized();
        const Vec3 s = Vec3::cross(f, up).normalized();
        const Vec3 u = Vec3::cross(s, f);

        Mat4 m;
        m(0, 0) = s.x;  m(0, 1) = s.y;  m(0, 2) = s.z;  m(0, 3) = 0.0f;
        m(1, 0) = u.x;  m(1, 1) = u.y;  m(1, 2) = u.z;  m(1, 3) = 0.0f;
        m(2, 0) = -f.x; m(2, 1) = -f.y; m(2, 2) = -f.z; m(2, 3) = 0.0f;
        m(3, 0) = -Vec3::dot(s, eye);
        m(3, 1) = -Vec3::dot(u, eye);
        m(3, 2) = Vec3::dot(f, eye);
        m(3, 3) = 1.0f;

        return m;
    }

    [[nodiscard]] static Mat4 translation(float x, float y, float z) noexcept {
        Mat4 m;
        m(0, 3) = x;
        m(1, 3) = y;
        m(2, 3) = z;
        return m;
    }

    [[nodiscard]] static Mat4 translation(const Vec3& v) noexcept {
        return translation(v.x, v.y, v.z);
    }
};

} // namespace math

// =============================================================================
// DOUBLE-PRECISION WORLD POSITION
// Used for camera position to maintain precision at large coordinates
// =============================================================================
struct WorldPosition {
    double x, y, z;

    constexpr WorldPosition() noexcept : x(0), y(0), z(0) {}
    constexpr WorldPosition(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

    // Convert from chunk coordinates
    static WorldPosition from_chunk(ChunkCoord cx, ChunkCoord cy, ChunkCoord cz, double local_x = 0, double local_y = 0, double local_z = 0) noexcept {
        return {
            static_cast<double>(coord::chunk_to_world(cx)) + local_x,
            static_cast<double>(coord::chunk_to_world(cy)) + local_y,
            static_cast<double>(coord::chunk_to_world(cz)) + local_z
        };
    }

    // Get chunk position containing this world position
    [[nodiscard]] ChunkPosition to_chunk_pos() const noexcept {
        return {
            coord::world_to_chunk(static_cast<ChunkCoord>(std::floor(x))),
            coord::world_to_chunk(static_cast<ChunkCoord>(std::floor(y))),
            coord::world_to_chunk(static_cast<ChunkCoord>(std::floor(z)))
        };
    }

    constexpr WorldPosition operator+(const WorldPosition& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    constexpr WorldPosition operator-(const WorldPosition& other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
};

// =============================================================================
// CAMERA
// First-person camera with origin shifting for precision at world boundaries
// =============================================================================
class Camera {
public:
    // Camera movement directions
    enum class Direction {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    // Default values
    static constexpr float DEFAULT_YAW = -90.0f;
    static constexpr float DEFAULT_PITCH = 0.0f;
    static constexpr float DEFAULT_SPEED = 10.0f;
    static constexpr float DEFAULT_SENSITIVITY = 0.1f;
    static constexpr float DEFAULT_FOV = 70.0f;
    static constexpr float DEFAULT_NEAR = 0.1f;
    static constexpr float DEFAULT_FAR = 1000.0f;

    Camera();
    Camera(const WorldPosition& position, float yaw = DEFAULT_YAW, float pitch = DEFAULT_PITCH);

    // =============================================================================
    // POSITION & ORIENTATION
    // =============================================================================

    // Set position in world coordinates (double precision)
    void set_position(const WorldPosition& pos) noexcept;
    void set_position(double x, double y, double z) noexcept;

    [[nodiscard]] const WorldPosition& position() const noexcept { return m_position; }

    // Yaw/Pitch (degrees)
    void set_yaw(float yaw) noexcept;
    void set_pitch(float pitch) noexcept;
    [[nodiscard]] float yaw() const noexcept { return m_yaw; }
    [[nodiscard]] float pitch() const noexcept { return m_pitch; }

    // Set rotation (convenience method)
    void set_rotation(float pitch_deg, float yaw_deg) noexcept {
        m_pitch = pitch_deg;
        m_yaw = yaw_deg;
        update_vectors();
    }

    // =============================================================================
    // ORIGIN SHIFTING (Key for jitter-free rendering)
    // =============================================================================

    // Get/set the rendering origin (updated when camera moves far from origin)
    [[nodiscard]] const WorldPosition& render_origin() const noexcept { return m_render_origin; }
    void set_render_origin(const WorldPosition& origin) noexcept;

    // Get camera position relative to render origin (single precision, for GPU)
    [[nodiscard]] math::Vec3 relative_position() const noexcept;

    // Update origin if camera has moved too far from current origin
    // Returns true if origin was shifted
    bool update_origin_if_needed(double threshold = 1024.0);

    // =============================================================================
    // MATRICES
    // =============================================================================

    // Get view matrix (uses relative position for precision)
    [[nodiscard]] math::Mat4 view_matrix() const noexcept;

    // Get projection matrix
    [[nodiscard]] math::Mat4 projection_matrix() const noexcept;

    // Get combined view-projection matrix
    [[nodiscard]] math::Mat4 view_projection_matrix() const noexcept;

    // =============================================================================
    // PROJECTION SETTINGS
    // =============================================================================

    void set_fov(float fov_degrees) noexcept;
    void set_aspect_ratio(float aspect) noexcept;
    void set_near_far(float near, float far) noexcept;

    // Set projection (convenience method)
    void set_projection(float fov_degrees, float aspect, float near, float far) noexcept {
        m_fov = fov_degrees;
        m_aspect = aspect;
        m_near = near;
        m_far = far;
        m_projection_dirty = true;
    }

    [[nodiscard]] float fov() const noexcept { return m_fov; }
    [[nodiscard]] float aspect_ratio() const noexcept { return m_aspect; }
    [[nodiscard]] float near_plane() const noexcept { return m_near; }
    [[nodiscard]] float far_plane() const noexcept { return m_far; }

    // =============================================================================
    // INPUT HANDLING
    // =============================================================================

    // Process keyboard input
    void process_keyboard(Direction direction, float delta_time);

    // Process mouse movement
    void process_mouse(float x_offset, float y_offset, bool constrain_pitch = true);

    // Movement speed
    void set_speed(float speed) noexcept { m_speed = speed; }
    [[nodiscard]] float speed() const noexcept { return m_speed; }

    // Mouse sensitivity
    void set_sensitivity(float sensitivity) noexcept { m_sensitivity = sensitivity; }
    [[nodiscard]] float sensitivity() const noexcept { return m_sensitivity; }

    // =============================================================================
    // DIRECTION VECTORS
    // =============================================================================

    [[nodiscard]] const math::Vec3& front() const noexcept { return m_front; }
    [[nodiscard]] const math::Vec3& right() const noexcept { return m_right; }
    [[nodiscard]] const math::Vec3& up() const noexcept { return m_up; }

private:
    void update_vectors();

private:
    // World-space position (double precision for large coordinates)
    WorldPosition m_position;

    // Render origin for origin shifting
    WorldPosition m_render_origin;

    // Euler angles (degrees)
    float m_yaw;
    float m_pitch;

    // Direction vectors
    math::Vec3 m_front;
    math::Vec3 m_right;
    math::Vec3 m_up;
    static constexpr math::Vec3 WORLD_UP{0.0f, 1.0f, 0.0f};

    // Projection parameters
    float m_fov;
    float m_aspect;
    float m_near;
    float m_far;

    // Input parameters
    float m_speed;
    float m_sensitivity;

    // Cached matrices
    mutable math::Mat4 m_view_matrix;
    mutable math::Mat4 m_projection_matrix;
    mutable bool m_view_dirty = true;
    mutable bool m_projection_dirty = true;
};

} // namespace voxel::client
