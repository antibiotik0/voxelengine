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

    // Move entity with collision detection
    // Returns the actual movement (may be less than requested if collision)
    template<typename GetVoxelFn>
    static void move_with_collision(
        double& x, double& y, double& z,
        double dx, double dy, double dz,
        double half_width, double half_height,
        GetVoxelFn&& get_voxel,
        bool& on_ground
    ) {
        on_ground = false;

        // Small epsilon for collision padding
        constexpr double EPSILON = 0.001;

        // Try Y movement first (gravity)
        if (std::abs(dy) > EPSILON) {
            double step = (dy > 0) ? 0.1 : -0.1;
            double remaining = dy;

            while (std::abs(remaining) > EPSILON) {
                double move = (std::abs(remaining) < std::abs(step)) ? remaining : step;
                double new_y = y + move;

                if (!would_collide(x, new_y, z, half_width, half_height, get_voxel)) {
                    y = new_y;
                    remaining -= move;
                } else {
                    if (dy < 0) {
                        on_ground = true;
                    }
                    break;
                }
            }
        }

        // Try X movement
        if (std::abs(dx) > EPSILON) {
            double step = (dx > 0) ? 0.1 : -0.1;
            double remaining = dx;

            while (std::abs(remaining) > EPSILON) {
                double move = (std::abs(remaining) < std::abs(step)) ? remaining : step;
                double new_x = x + move;

                if (!would_collide(new_x, y, z, half_width, half_height, get_voxel)) {
                    x = new_x;
                    remaining -= move;
                } else {
                    break;
                }
            }
        }

        // Try Z movement
        if (std::abs(dz) > EPSILON) {
            double step = (dz > 0) ? 0.1 : -0.1;
            double remaining = dz;

            while (std::abs(remaining) > EPSILON) {
                double move = (std::abs(remaining) < std::abs(step)) ? remaining : step;
                double new_z = z + move;

                if (!would_collide(x, y, new_z, half_width, half_height, get_voxel)) {
                    z = new_z;
                    remaining -= move;
                } else {
                    break;
                }
            }
        }
    }
};

} // namespace voxel
