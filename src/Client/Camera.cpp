// =============================================================================
// VOXEL ENGINE - CAMERA IMPLEMENTATION
// Origin shifting for jitter-free rendering at extreme coordinates
// =============================================================================

#include "Client/Camera.hpp"

#include <cmath>
#include <algorithm>

namespace voxel::client {

// =============================================================================
// CONSTRUCTION
// =============================================================================

Camera::Camera()
    : m_position{0, 0, 0}
    , m_render_origin{0, 0, 0}
    , m_yaw(DEFAULT_YAW)
    , m_pitch(DEFAULT_PITCH)
    , m_front{0, 0, -1}
    , m_right{1, 0, 0}
    , m_up{0, 1, 0}
    , m_fov(DEFAULT_FOV)
    , m_aspect(16.0f / 9.0f)
    , m_near(DEFAULT_NEAR)
    , m_far(DEFAULT_FAR)
    , m_speed(DEFAULT_SPEED)
    , m_sensitivity(DEFAULT_SENSITIVITY)
{
    update_vectors();
}

Camera::Camera(const WorldPosition& position, float yaw, float pitch)
    : m_position(position)
    , m_render_origin(position)
    , m_yaw(yaw)
    , m_pitch(pitch)
    , m_front{0, 0, -1}
    , m_right{1, 0, 0}
    , m_up{0, 1, 0}
    , m_fov(DEFAULT_FOV)
    , m_aspect(16.0f / 9.0f)
    , m_near(DEFAULT_NEAR)
    , m_far(DEFAULT_FAR)
    , m_speed(DEFAULT_SPEED)
    , m_sensitivity(DEFAULT_SENSITIVITY)
{
    update_vectors();
}

// =============================================================================
// POSITION & ORIENTATION
// =============================================================================

void Camera::set_position(const WorldPosition& pos) noexcept {
    m_position = pos;
    m_view_dirty = true;
}

void Camera::set_position(double x, double y, double z) noexcept {
    m_position = {x, y, z};
    m_view_dirty = true;
}

void Camera::set_yaw(float yaw) noexcept {
    m_yaw = yaw;
    update_vectors();
}

void Camera::set_pitch(float pitch) noexcept {
    m_pitch = std::clamp(pitch, -89.0f, 89.0f);
    update_vectors();
}

// =============================================================================
// ORIGIN SHIFTING
// =============================================================================

void Camera::set_render_origin(const WorldPosition& origin) noexcept {
    m_render_origin = origin;
    m_view_dirty = true;
}

math::Vec3 Camera::relative_position() const noexcept {
    // Calculate camera position relative to render origin
    // This gives us single-precision coordinates near the origin
    return {
        static_cast<float>(m_position.x - m_render_origin.x),
        static_cast<float>(m_position.y - m_render_origin.y),
        static_cast<float>(m_position.z - m_render_origin.z)
    };
}

bool Camera::update_origin_if_needed(double threshold) {
    const double dx = m_position.x - m_render_origin.x;
    const double dy = m_position.y - m_render_origin.y;
    const double dz = m_position.z - m_render_origin.z;

    const double distance_sq = dx * dx + dy * dy + dz * dz;

    if (distance_sq > threshold * threshold) {
        // Shift origin to camera position (snapped to chunk boundaries for stability)
        m_render_origin.x = std::floor(m_position.x / 64.0) * 64.0;
        m_render_origin.y = std::floor(m_position.y / 64.0) * 64.0;
        m_render_origin.z = std::floor(m_position.z / 64.0) * 64.0;
        m_view_dirty = true;
        return true;
    }
    return false;
}

// =============================================================================
// MATRICES
// =============================================================================

math::Mat4 Camera::view_matrix() const noexcept {
    if (m_view_dirty) {
        // Use relative position for view matrix calculation
        const math::Vec3 pos = relative_position();
        const math::Vec3 target = pos + m_front;
        m_view_matrix = math::Mat4::look_at(pos, target, WORLD_UP);
        m_view_dirty = false;
    }
    return m_view_matrix;
}

math::Mat4 Camera::projection_matrix() const noexcept {
    if (m_projection_dirty) {
        m_projection_matrix = math::Mat4::perspective(
            m_fov * math::DEG_TO_RAD,
            m_aspect,
            m_near,
            m_far
        );
        m_projection_dirty = false;
    }
    return m_projection_matrix;
}

math::Mat4 Camera::view_projection_matrix() const noexcept {
    return projection_matrix() * view_matrix();
}

// =============================================================================
// PROJECTION SETTINGS
// =============================================================================

void Camera::set_fov(float fov_degrees) noexcept {
    m_fov = std::clamp(fov_degrees, 1.0f, 179.0f);
    m_projection_dirty = true;
}

void Camera::set_aspect_ratio(float aspect) noexcept {
    m_aspect = aspect;
    m_projection_dirty = true;
}

void Camera::set_near_far(float near, float far) noexcept {
    m_near = near;
    m_far = far;
    m_projection_dirty = true;
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

void Camera::process_keyboard(Direction direction, float delta_time) {
    const double velocity = static_cast<double>(m_speed * delta_time);

    switch (direction) {
        case Direction::FORWARD:
            m_position.x += static_cast<double>(m_front.x) * velocity;
            m_position.y += static_cast<double>(m_front.y) * velocity;
            m_position.z += static_cast<double>(m_front.z) * velocity;
            break;
        case Direction::BACKWARD:
            m_position.x -= static_cast<double>(m_front.x) * velocity;
            m_position.y -= static_cast<double>(m_front.y) * velocity;
            m_position.z -= static_cast<double>(m_front.z) * velocity;
            break;
        case Direction::LEFT:
            m_position.x -= static_cast<double>(m_right.x) * velocity;
            m_position.y -= static_cast<double>(m_right.y) * velocity;
            m_position.z -= static_cast<double>(m_right.z) * velocity;
            break;
        case Direction::RIGHT:
            m_position.x += static_cast<double>(m_right.x) * velocity;
            m_position.y += static_cast<double>(m_right.y) * velocity;
            m_position.z += static_cast<double>(m_right.z) * velocity;
            break;
        case Direction::UP:
            m_position.y += velocity;
            break;
        case Direction::DOWN:
            m_position.y -= velocity;
            break;
    }

    m_view_dirty = true;
}

void Camera::process_mouse(float x_offset, float y_offset, bool constrain_pitch) {
    x_offset *= m_sensitivity;
    y_offset *= m_sensitivity;

    m_yaw += x_offset;
    m_pitch += y_offset;

    if (constrain_pitch) {
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    }

    update_vectors();
}

// =============================================================================
// PRIVATE METHODS
// =============================================================================

void Camera::update_vectors() {
    // Calculate front vector from Euler angles
    const float yaw_rad = m_yaw * math::DEG_TO_RAD;
    const float pitch_rad = m_pitch * math::DEG_TO_RAD;

    m_front.x = std::cos(yaw_rad) * std::cos(pitch_rad);
    m_front.y = std::sin(pitch_rad);
    m_front.z = std::sin(yaw_rad) * std::cos(pitch_rad);
    m_front = m_front.normalized();

    // Recalculate right and up vectors
    m_right = math::Vec3::cross(m_front, WORLD_UP).normalized();
    m_up = math::Vec3::cross(m_right, m_front).normalized();

    m_view_dirty = true;
}

} // namespace voxel::client
