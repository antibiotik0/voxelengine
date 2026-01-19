// =============================================================================
// VOXEL ENGINE - FLUID SIMULATOR
// Cellular Automata Fluid Simulation (4 TPS)
// Uses BlockRegistry for fluid properties (Data-Driven)
// =============================================================================
#pragma once

#include "Shared/Types.hpp"
#include "Shared/BlockRegistry.hpp"
#include "Server/World.hpp"

#include <vector>
#include <queue>
#include <unordered_set>
#include <mutex>

namespace voxel::server {

// =============================================================================
// FLUID UPDATE ENTRY
// =============================================================================
struct FluidUpdate {
    ChunkCoord x, y, z;
    std::uint16_t fluid_id;
    std::uint8_t level;  // 0 = source, 1-7 = flowing distance
};

// =============================================================================
// FLUID SIMULATOR
// Runs at 4 TPS (every 5 simulation ticks at 20 TPS)
// =============================================================================
class FluidSimulator {
public:
    static constexpr int FLUID_UPDATE_INTERVAL = 5;  // Ticks between fluid updates (4 TPS)
    
    explicit FluidSimulator(World& world) 
        : m_world(world), m_tick_counter(0) {}
    
    // =============================================================================
    // MAIN UPDATE (Called from TickManager)
    // =============================================================================
    void tick() {
        m_tick_counter++;
        
        // Only update fluids every FLUID_UPDATE_INTERVAL ticks
        if (m_tick_counter < FLUID_UPDATE_INTERVAL) {
            return;
        }
        m_tick_counter = 0;
        
        // Process pending fluid updates
        process_updates();
    }
    
    // =============================================================================
    // SCHEDULE FLUID UPDATE
    // Called when a fluid block is placed or adjacent block changes
    // =============================================================================
    void schedule_update(ChunkCoord x, ChunkCoord y, ChunkCoord z) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Get block at position
        Voxel voxel = m_world.get_voxel(x, y, z);
        const auto& props = BlockRegistry::instance().get(voxel.type_id());
        
        if (props.is_fluid) {
            m_pending.push({x, y, z, voxel.type_id(), voxel.metadata()});
        }
    }
    
    // Schedule updates for all fluids around a changed block
    void notify_block_change(ChunkCoord x, ChunkCoord y, ChunkCoord z) {
        // Check neighbors for fluids
        static constexpr std::array<std::array<ChunkCoord, 3>, 6> offsets = {{
            {-1, 0, 0}, {1, 0, 0},
            {0, -1, 0}, {0, 1, 0},
            {0, 0, -1}, {0, 0, 1}
        }};
        
        for (const auto& off : offsets) {
            ChunkCoord nx = x + off[0];
            ChunkCoord ny = y + off[1];
            ChunkCoord nz = z + off[2];
            
            Voxel neighbor = m_world.get_voxel(nx, ny, nz);
            if (BlockRegistry::instance().is_fluid(neighbor.type_id())) {
                schedule_update(nx, ny, nz);
            }
        }
        
        // Also check if the block above is a fluid (for downward flow)
        Voxel above = m_world.get_voxel(x, y + 1, z);
        if (BlockRegistry::instance().is_fluid(above.type_id())) {
            schedule_update(x, y + 1, z);
        }
    }

private:
    void process_updates() {
        std::queue<FluidUpdate> to_process;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::swap(to_process, m_pending);
        }
        
        std::unordered_set<std::size_t> processed;
        
        while (!to_process.empty()) {
            FluidUpdate update = to_process.front();
            to_process.pop();
            
            // Hash position to avoid duplicate processing
            std::size_t hash = position_hash(update.x, update.y, update.z);
            if (processed.count(hash) > 0) continue;
            processed.insert(hash);
            
            simulate_fluid(update);
        }
    }
    
    void simulate_fluid(const FluidUpdate& update) {
        Voxel current = m_world.get_voxel(update.x, update.y, update.z);
        
        // Skip if no longer a fluid
        if (!BlockRegistry::instance().is_fluid(current.type_id())) {
            return;
        }
        
        const auto& props = BlockRegistry::instance().get(current.type_id());
        std::uint8_t current_level = current.metadata();
        
        // Check block below - flow down first (priority)
        Voxel below = m_world.get_voxel(update.x, update.y - 1, update.z);
        if (can_flow_into(below)) {
            // Flow down - becomes source-like (level 0 for infinite flow down)
            Voxel new_fluid(current.type_id());
            new_fluid.set_metadata(0);  // Reset level when flowing down
            m_world.set_voxel(update.x, update.y - 1, update.z, new_fluid);
            
            // Schedule the new fluid for update
            schedule_update(update.x, update.y - 1, update.z);
            return;
        }
        
        // If blocked below, spread horizontally
        if (current_level < props.fluid_max_distance) {
            spread_horizontal(update.x, update.y, update.z, current.type_id(), 
                            current_level, props.fluid_max_distance);
        }
        
        // Check if this flowing water should disappear (no source feeding it)
        if (current_level > 0) {
            if (!has_fluid_source_nearby(update.x, update.y, update.z, current.type_id())) {
                // Remove flowing water that lost its source
                m_world.set_voxel(update.x, update.y, update.z, Voxel(VoxelType::AIR));
            }
        }
    }
    
    void spread_horizontal(ChunkCoord x, ChunkCoord y, ChunkCoord z, 
                          std::uint16_t fluid_id, std::uint8_t current_level,
                          std::uint8_t max_distance) {
        static constexpr std::array<std::array<ChunkCoord, 2>, 4> horizontal = {{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}
        }};
        
        std::uint8_t new_level = static_cast<std::uint8_t>(current_level + 1);
        if (new_level > max_distance) return;
        
        for (const auto& dir : horizontal) {
            ChunkCoord nx = x + dir[0];
            ChunkCoord nz = z + dir[1];
            
            Voxel neighbor = m_world.get_voxel(nx, y, nz);
            
            if (can_flow_into(neighbor)) {
                // Place flowing water
                Voxel new_fluid(fluid_id);
                new_fluid.set_metadata(new_level);
                m_world.set_voxel(nx, y, nz, new_fluid);
                schedule_update(nx, y, nz);
            } else if (neighbor.type_id() == fluid_id) {
                // Existing fluid - check if we provide a shorter path
                if (neighbor.metadata() > new_level && new_level > 0) {
                    Voxel updated_fluid(fluid_id);
                    updated_fluid.set_metadata(new_level);
                    m_world.set_voxel(nx, y, nz, updated_fluid);
                    schedule_update(nx, y, nz);
                }
            }
        }
    }
    
    [[nodiscard]] bool can_flow_into(const Voxel& target) const {
        if (target.is_air()) return true;
        
        const auto& props = BlockRegistry::instance().get(target.type_id());
        return !props.is_solid && !props.is_fluid;
    }
    
    [[nodiscard]] bool has_fluid_source_nearby(ChunkCoord x, ChunkCoord y, ChunkCoord z,
                                               std::uint16_t fluid_id) const {
        // Check above (falling water is always fed)
        Voxel above = m_world.get_voxel(x, y + 1, z);
        if (above.type_id() == fluid_id) {
            return true;
        }
        
        // Check horizontal neighbors for source or lower-level fluid
        Voxel current = m_world.get_voxel(x, y, z);
        std::uint8_t current_level = current.metadata();
        
        static constexpr std::array<std::array<ChunkCoord, 2>, 4> horizontal = {{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}
        }};
        
        for (const auto& dir : horizontal) {
            Voxel neighbor = m_world.get_voxel(x + dir[0], y, z + dir[1]);
            if (neighbor.type_id() == fluid_id) {
                // Source block (level 0) or block with lower level feeds us
                if (neighbor.metadata() < current_level) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    [[nodiscard]] static std::size_t position_hash(ChunkCoord x, ChunkCoord y, ChunkCoord z) {
        std::size_t h = 14695981039346656037ULL;
        h ^= static_cast<std::size_t>(x);
        h *= 1099511628211ULL;
        h ^= static_cast<std::size_t>(y);
        h *= 1099511628211ULL;
        h ^= static_cast<std::size_t>(z);
        return h;
    }
    
    World& m_world;
    std::queue<FluidUpdate> m_pending;
    std::mutex m_mutex;
    int m_tick_counter;
};

} // namespace voxel::server
