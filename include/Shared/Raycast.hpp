// =============================================================================
// VOXEL ENGINE - AMANATIDES-WOO VOXEL RAYCASTER
// Zero heap allocations, cache-friendly traversal
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include <cmath>
#include <cstdint>

namespace voxel {

// =============================================================================
// RAYCAST RESULT (Stack-allocated, no heap)
// =============================================================================
struct RaycastHit {
    // Block position (world coordinates)
    std::int64_t block_x = 0;
    std::int64_t block_y = 0;
    std::int64_t block_z = 0;

    // Face normal (which face was hit)
    std::int32_t normal_x = 0;
    std::int32_t normal_y = 0;
    std::int32_t normal_z = 0;

    // Distance from origin to hit point
    float distance = 0.0f;

    // The voxel that was hit
    Voxel hit_voxel;

    // Whether a block was hit
    bool hit = false;
};

// =============================================================================
// AMANATIDES-WOO VOXEL RAYCASTER
// Paper: "A Fast Voxel Traversal Algorithm for Ray Tracing" (1987)
// =============================================================================
class VoxelRaycaster {
public:
    // Callback type for getting voxel at world position
    // Must be a lightweight function (no std::function to avoid heap)
    template<typename GetVoxelFn>
    static RaycastHit cast(
        double origin_x, double origin_y, double origin_z,
        float dir_x, float dir_y, float dir_z,
        float max_distance,
        GetVoxelFn&& get_voxel
    ) {
        RaycastHit result;

        // Normalize direction
        const float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
        if (dir_len < 0.0001f) return result;

        dir_x /= dir_len;
        dir_y /= dir_len;
        dir_z /= dir_len;

        // Current voxel position (use int64_t for large world support)
        std::int64_t x = static_cast<std::int64_t>(std::floor(origin_x));
        std::int64_t y = static_cast<std::int64_t>(std::floor(origin_y));
        std::int64_t z = static_cast<std::int64_t>(std::floor(origin_z));

        // Step direction (+1 or -1)
        const std::int32_t step_x = (dir_x >= 0.0f) ? 1 : -1;
        const std::int32_t step_y = (dir_y >= 0.0f) ? 1 : -1;
        const std::int32_t step_z = (dir_z >= 0.0f) ? 1 : -1;

        // tDelta: how far along ray to move for one voxel in each axis
        const float t_delta_x = (std::abs(dir_x) > 0.0001f) ? std::abs(1.0f / dir_x) : 1e30f;
        const float t_delta_y = (std::abs(dir_y) > 0.0001f) ? std::abs(1.0f / dir_y) : 1e30f;
        const float t_delta_z = (std::abs(dir_z) > 0.0001f) ? std::abs(1.0f / dir_z) : 1e30f;

        // tMax: distance to next voxel boundary in each axis
        float t_max_x, t_max_y, t_max_z;

        if (dir_x >= 0.0f) {
            t_max_x = (static_cast<float>(x + 1) - static_cast<float>(origin_x)) * t_delta_x;
        } else {
            t_max_x = (static_cast<float>(origin_x) - static_cast<float>(x)) * t_delta_x;
        }

        if (dir_y >= 0.0f) {
            t_max_y = (static_cast<float>(y + 1) - static_cast<float>(origin_y)) * t_delta_y;
        } else {
            t_max_y = (static_cast<float>(origin_y) - static_cast<float>(y)) * t_delta_y;
        }

        if (dir_z >= 0.0f) {
            t_max_z = (static_cast<float>(z + 1) - static_cast<float>(origin_z)) * t_delta_z;
        } else {
            t_max_z = (static_cast<float>(origin_z) - static_cast<float>(z)) * t_delta_z;
        }

        // Track which axis we stepped on (for face normal)
        // 0 = X, 1 = Y, 2 = Z
        std::int32_t last_axis = -1;

        float distance = 0.0f;

        // Amanatides-Woo traversal loop
        while (distance < max_distance) {
            // Check current voxel
            Voxel voxel = get_voxel(x, y, z);
            
            if (!voxel.is_air()) {
                result.hit = true;
                result.block_x = x;
                result.block_y = y;
                result.block_z = z;
                result.distance = distance;
                result.hit_voxel = voxel;

                // Set face normal based on last step
                result.normal_x = 0;
                result.normal_y = 0;
                result.normal_z = 0;

                if (last_axis == 0) result.normal_x = -step_x;
                else if (last_axis == 1) result.normal_y = -step_y;
                else if (last_axis == 2) result.normal_z = -step_z;

                return result;
            }

            // Step to next voxel (Amanatides-Woo core algorithm)
            if (t_max_x < t_max_y) {
                if (t_max_x < t_max_z) {
                    x += step_x;
                    distance = t_max_x;
                    t_max_x += t_delta_x;
                    last_axis = 0;
                } else {
                    z += step_z;
                    distance = t_max_z;
                    t_max_z += t_delta_z;
                    last_axis = 2;
                }
            } else {
                if (t_max_y < t_max_z) {
                    y += step_y;
                    distance = t_max_y;
                    t_max_y += t_delta_y;
                    last_axis = 1;
                } else {
                    z += step_z;
                    distance = t_max_z;
                    t_max_z += t_delta_z;
                    last_axis = 2;
                }
            }
        }

        return result; // No hit
    }

    // Convenience overload for Vec3 types
    template<typename Vec3Origin, typename Vec3Dir, typename GetVoxelFn>
    static RaycastHit cast(
        const Vec3Origin& origin,
        const Vec3Dir& direction,
        float max_distance,
        GetVoxelFn&& get_voxel
    ) {
        return cast(
            static_cast<double>(origin.x),
            static_cast<double>(origin.y),
            static_cast<double>(origin.z),
            static_cast<float>(direction.x),
            static_cast<float>(direction.y),
            static_cast<float>(direction.z),
            max_distance,
            std::forward<GetVoxelFn>(get_voxel)
        );
    }
};

} // namespace voxel
