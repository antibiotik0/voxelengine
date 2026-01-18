// =============================================================================
// VOXEL ENGINE - BLOCK RAYCASTING
// DDA algorithm for finding the block the player is looking at
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include "Camera.hpp"

#include <cmath>
#include <optional>
#include <functional>

namespace voxel::client {

// Result of a block raycast
struct RaycastResult {
    // Block position that was hit
    std::int32_t block_x;
    std::int32_t block_y;
    std::int32_t block_z;

    // Face normal of the hit (which face was hit)
    std::int32_t normal_x;
    std::int32_t normal_y;
    std::int32_t normal_z;

    // Distance to hit point
    float distance;

    // World position of hit point
    float hit_x;
    float hit_y;
    float hit_z;
};

// =============================================================================
// BLOCK RAYCASTER
// Uses DDA (Digital Differential Analyzer) algorithm
// =============================================================================
class BlockRaycaster {
public:
    // Function type that returns a Voxel at given world coordinates
    using GetVoxelFunc = std::function<Voxel(std::int32_t, std::int32_t, std::int32_t)>;

    // Perform raycast from camera position in camera direction
    // Returns the first non-air block hit within max_distance
    static std::optional<RaycastResult> cast(
        const math::Vec3& origin,
        const math::Vec3& direction,
        float max_distance,
        GetVoxelFunc get_voxel
    ) {
        // Normalize direction
        const float dir_len = std::sqrt(direction.x * direction.x + 
                                         direction.y * direction.y + 
                                         direction.z * direction.z);
        if (dir_len < 0.0001f) return std::nullopt;

        const float dir_x = direction.x / dir_len;
        const float dir_y = direction.y / dir_len;
        const float dir_z = direction.z / dir_len;

        // Current block position
        std::int32_t block_x = static_cast<std::int32_t>(std::floor(origin.x));
        std::int32_t block_y = static_cast<std::int32_t>(std::floor(origin.y));
        std::int32_t block_z = static_cast<std::int32_t>(std::floor(origin.z));

        // Direction to step in each axis
        const std::int32_t step_x = (dir_x >= 0) ? 1 : -1;
        const std::int32_t step_y = (dir_y >= 0) ? 1 : -1;
        const std::int32_t step_z = (dir_z >= 0) ? 1 : -1;

        // Distance along ray to next voxel boundary
        const float t_delta_x = (std::abs(dir_x) > 0.0001f) ? std::abs(1.0f / dir_x) : 1e30f;
        const float t_delta_y = (std::abs(dir_y) > 0.0001f) ? std::abs(1.0f / dir_y) : 1e30f;
        const float t_delta_z = (std::abs(dir_z) > 0.0001f) ? std::abs(1.0f / dir_z) : 1e30f;

        // Distance to first voxel boundary
        float t_max_x, t_max_y, t_max_z;
        if (dir_x >= 0) {
            t_max_x = (static_cast<float>(block_x + 1) - origin.x) * t_delta_x;
        } else {
            t_max_x = (origin.x - static_cast<float>(block_x)) * t_delta_x;
        }
        if (dir_y >= 0) {
            t_max_y = (static_cast<float>(block_y + 1) - origin.y) * t_delta_y;
        } else {
            t_max_y = (origin.y - static_cast<float>(block_y)) * t_delta_y;
        }
        if (dir_z >= 0) {
            t_max_z = (static_cast<float>(block_z + 1) - origin.z) * t_delta_z;
        } else {
            t_max_z = (origin.z - static_cast<float>(block_z)) * t_delta_z;
        }

        // Track last step direction for face normal
        std::int32_t last_step_axis = -1; // 0=x, 1=y, 2=z

        float distance = 0.0f;

        // DDA loop
        while (distance < max_distance) {
            // Check current block
            Voxel voxel = get_voxel(block_x, block_y, block_z);
            if (!voxel.is_air()) {
                RaycastResult result;
                result.block_x = block_x;
                result.block_y = block_y;
                result.block_z = block_z;
                result.distance = distance;
                result.hit_x = origin.x + dir_x * distance;
                result.hit_y = origin.y + dir_y * distance;
                result.hit_z = origin.z + dir_z * distance;

                // Set face normal based on last step
                result.normal_x = 0;
                result.normal_y = 0;
                result.normal_z = 0;
                if (last_step_axis == 0) result.normal_x = -step_x;
                else if (last_step_axis == 1) result.normal_y = -step_y;
                else if (last_step_axis == 2) result.normal_z = -step_z;

                return result;
            }

            // Step to next voxel
            if (t_max_x < t_max_y) {
                if (t_max_x < t_max_z) {
                    block_x += step_x;
                    distance = t_max_x;
                    t_max_x += t_delta_x;
                    last_step_axis = 0;
                } else {
                    block_z += step_z;
                    distance = t_max_z;
                    t_max_z += t_delta_z;
                    last_step_axis = 2;
                }
            } else {
                if (t_max_y < t_max_z) {
                    block_y += step_y;
                    distance = t_max_y;
                    t_max_y += t_delta_y;
                    last_step_axis = 1;
                } else {
                    block_z += step_z;
                    distance = t_max_z;
                    t_max_z += t_delta_z;
                    last_step_axis = 2;
                }
            }
        }

        return std::nullopt;
    }
};

} // namespace voxel::client
