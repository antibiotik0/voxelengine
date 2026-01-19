// =============================================================================
// VOXEL ENGINE - AABB COLLISION DETECTION
// Basic Axis-Aligned Bounding Box collision for player movement
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include <cmath>
#include <algorithm>

namespace voxel {

// =============================================================================
// AXIS-ALIGNED BOUNDING BOX
// =============================================================================
struct AABB {
    double min_x, min_y, min_z;
    double max_x, max_y, max_z;

    // Create AABB from center and half-extents
    static AABB from_center(double cx, double cy, double cz, 
                            double half_width, double half_height, double half_depth) {
        return AABB{
            cx - half_width, cy - half_height, cz - half_depth,
            cx + half_width, cy + half_height, cz + half_depth
        };
    }

    // Create AABB for a block at world position
    static AABB from_block(std::int64_t x, std::int64_t y, std::int64_t z) {
        return AABB{
            static_cast<double>(x), static_cast<double>(y), static_cast<double>(z),
            static_cast<double>(x + 1), static_cast<double>(y + 1), static_cast<double>(z + 1)
        };
    }

    // Check if two AABBs intersect
    [[nodiscard]] bool intersects(const AABB& other) const noexcept {
        return (max_x > other.min_x && min_x < other.max_x) &&
               (max_y > other.min_y && min_y < other.max_y) &&
               (max_z > other.min_z && min_z < other.max_z);
    }

    // Move AABB by offset
    [[nodiscard]] AABB offset(double dx, double dy, double dz) const noexcept {
        return AABB{
            min_x + dx, min_y + dy, min_z + dz,
            max_x + dx, max_y + dy, max_z + dz
        };
    }

    // Expand AABB by amount in all directions
    [[nodiscard]] AABB expand(double amount) const noexcept {
        return AABB{
            min_x - amount, min_y - amount, min_z - amount,
            max_x + amount, max_y + amount, max_z + amount
        };
    }
};

// =============================================================================
// COLLISION RESOLVER
// Swept AABB collision detection and resolution
// =============================================================================
class CollisionResolver {
public:
    // Player collision box dimensions (Minecraft-like)
    static constexpr double PLAYER_WIDTH = 0.6;
    static constexpr double PLAYER_HEIGHT = 1.8;
    static constexpr double PLAYER_EYE_HEIGHT = 1.62;

    // Get voxel callback type
    template<typename GetVoxelFn>
    static bool would_collide(
        double x, double y, double z,
        double half_width, double half_height,
        GetVoxelFn&& get_voxel
    ) {
        // Create AABB for entity
        AABB entity = AABB::from_center(x, y + half_height, z, half_width, half_height, half_width);

        // Check all blocks the entity overlaps
        std::int64_t min_bx = static_cast<std::int64_t>(std::floor(entity.min_x));
        std::int64_t max_bx = static_cast<std::int64_t>(std::floor(entity.max_x));
        std::int64_t min_by = static_cast<std::int64_t>(std::floor(entity.min_y));
        std::int64_t max_by = static_cast<std::int64_t>(std::floor(entity.max_y));
        std::int64_t min_bz = static_cast<std::int64_t>(std::floor(entity.min_z));
        std::int64_t max_bz = static_cast<std::int64_t>(std::floor(entity.max_z));

        for (std::int64_t bx = min_bx; bx <= max_bx; ++bx) {
            for (std::int64_t by = min_by; by <= max_by; ++by) {
                for (std::int64_t bz = min_bz; bz <= max_bz; ++bz) {
                    Voxel voxel = get_voxel(bx, by, bz);
                    if (!voxel.is_air()) {
                        AABB block = AABB::from_block(bx, by, bz);
                        if (entity.intersects(block)) {
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    // Move entity with collision detection (axis-by-axis resolution for sliding)
    // Returns the actual position after collision
    template<typename GetVoxelFn>
    static void move_with_collision(
        double& x, double& y, double& z,
        double dx, double dy, double dz,
        double half_width, double half_height,
        GetVoxelFn&& get_voxel,
        bool& on_ground
    ) {
        on_ground = false;
        constexpr double EPSILON = 0.001;
        constexpr double STEP_SIZE = 0.05;  // Small steps for precision

        // === MOVE X AXIS ===
        if (std::abs(dx) > EPSILON) {
            double remaining_x = dx;
            int steps = static_cast<int>(std::ceil(std::abs(dx) / STEP_SIZE));
            double step_x = dx / steps;

            for (int i = 0; i < steps; ++i) {
                double test_x = x + step_x;
                if (!would_collide(test_x, y, z, half_width, half_height, get_voxel)) {
                    x = test_x;
                } else {
                    // Hit wall, stop X movement
                    break;
                }
            }
        }

        // === MOVE Y AXIS (Gravity) ===
        if (std::abs(dy) > EPSILON) {
            double remaining_y = dy;
            int steps = static_cast<int>(std::ceil(std::abs(dy) / STEP_SIZE));
            double step_y = dy / steps;

            for (int i = 0; i < steps; ++i) {
                double test_y = y + step_y;
                if (!would_collide(x, test_y, z, half_width, half_height, get_voxel)) {
                    y = test_y;
                } else {
                    // Hit floor/ceiling
                    if (dy < 0) {
                        on_ground = true;  // Landed on ground
                    }
                    break;
                }
            }
        }

        // === MOVE Z AXIS ===
        if (std::abs(dz) > EPSILON) {
            double remaining_z = dz;
            int steps = static_cast<int>(std::ceil(std::abs(dz) / STEP_SIZE));
            double step_z = dz / steps;

            for (int i = 0; i < steps; ++i) {
                double test_z = z + step_z;
                if (!would_collide(x, y, test_z, half_width, half_height, get_voxel)) {
                    z = test_z;
                } else {
                    // Hit wall, stop Z movement
                    break;
                }
            }
        }
    }
};

} // namespace voxel
