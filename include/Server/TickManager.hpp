// =============================================================================
// VOXEL ENGINE - TICK MANAGER
// Sacred Interface: Fixed-timestep simulation loop with temporal control
// Decoupled from render loop - supports variable simulation speed
// =============================================================================
#pragma once

#include "Shared/Types.hpp"

#include <chrono>
#include <functional>
#include <atomic>

namespace voxel::server {

// =============================================================================
// TIME TYPES
// =============================================================================
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = std::chrono::nanoseconds;
using Seconds = std::chrono::duration<double>;

// =============================================================================
// TICK MANAGER CONFIGURATION
// =============================================================================
struct TickConfig {
    // Target ticks per second (default: 20 TPS like Minecraft)
    std::uint32_t target_tps = 20;

    // Maximum ticks to process per frame (prevent spiral of death)
    std::uint32_t max_ticks_per_frame = 10;

    // Simulation speed multiplier (0.0 = frozen, 1.0 = normal, 2.0 = 2x speed)
    double simulation_speed = 1.0;

    // Fixed timestep duration in nanoseconds
    [[nodiscard]] constexpr Duration tick_duration() const noexcept {
        return Duration(static_cast<std::int64_t>(1'000'000'000.0 / target_tps));
    }

    // Fixed timestep in seconds (for physics calculations)
    [[nodiscard]] constexpr double delta_time() const noexcept {
        return 1.0 / static_cast<double>(target_tps);
    }
};

// =============================================================================
// TICK STATISTICS
// =============================================================================
struct TickStats {
    std::uint64_t total_ticks = 0;          // Total ticks since start
    std::uint64_t ticks_this_second = 0;    // Ticks in current second
    std::uint32_t current_tps = 0;          // Measured TPS
    double tick_time_ms = 0.0;              // Average tick execution time
    double accumulator_ms = 0.0;            // Current accumulator value
    bool is_running = false;                // Simulation running state
    bool is_lagging = false;                // True if can't keep up with target TPS
};

// =============================================================================
// TICK CALLBACK TYPES
// =============================================================================
using TickCallback = std::function<void(double delta_time, std::uint64_t tick_count)>;
using FrameCallback = std::function<void(double interpolation_alpha)>;

// =============================================================================
// TICK MANAGER
// Implements fixed-timestep game loop with accumulator pattern
// =============================================================================
class TickManager {
public:
    // =============================================================================
    // CONSTRUCTION
    // =============================================================================
    
    TickManager() 
        : m_config{}
        , m_stats{}
        , m_running(false)
        , m_accumulator(Duration::zero())
        , m_last_update(Clock::now())
        , m_last_stats_update(Clock::now())
        , m_ticks_this_second(0)
    {}

    explicit TickManager(TickConfig config)
        : m_config(config)
        , m_stats{}
        , m_running(false)
        , m_accumulator(Duration::zero())
        , m_last_update(Clock::now())
        , m_last_stats_update(Clock::now())
        , m_ticks_this_second(0)
    {}

    // Non-copyable, movable
    TickManager(const TickManager&) = delete;
    TickManager& operator=(const TickManager&) = delete;
    TickManager(TickManager&&) noexcept = default;
    TickManager& operator=(TickManager&&) noexcept = default;

    // =============================================================================
    // CONFIGURATION
    // =============================================================================

    void set_config(TickConfig config) noexcept {
        m_config = config;
    }

    [[nodiscard]] const TickConfig& config() const noexcept {
        return m_config;
    }

    void set_simulation_speed(double speed) noexcept {
        m_config.simulation_speed = speed < 0.0 ? 0.0 : speed;
    }

    [[nodiscard]] double simulation_speed() const noexcept {
        return m_config.simulation_speed;
    }

    void set_target_tps(std::uint32_t tps) noexcept {
        m_config.target_tps = tps > 0 ? tps : 1;
    }

    [[nodiscard]] std::uint32_t target_tps() const noexcept {
        return m_config.target_tps;
    }

    // =============================================================================
    // LIFECYCLE
    // =============================================================================

    void start() noexcept {
        m_running.store(true, std::memory_order_release);
        m_last_update = Clock::now();
        m_last_stats_update = Clock::now();
        m_accumulator = Duration::zero();
        m_ticks_this_second = 0;
        m_stats = TickStats{};
        m_stats.is_running = true;
    }

    void stop() noexcept {
        m_running.store(false, std::memory_order_release);
        m_stats.is_running = false;
    }

    void pause() noexcept {
        m_config.simulation_speed = 0.0;
    }

    void resume() noexcept {
        m_config.simulation_speed = 1.0;
    }

    [[nodiscard]] bool is_running() const noexcept {
        return m_running.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_paused() const noexcept {
        return m_config.simulation_speed == 0.0;
    }

    // =============================================================================
    // MAIN UPDATE LOOP
    // Call this once per frame from the main loop
    // Returns the interpolation alpha for rendering
    // =============================================================================

    [[nodiscard]] double update(TickCallback on_tick) {
        if (!is_running()) {
            return 0.0;
        }

        const TimePoint now = Clock::now();
        Duration frame_time = now - m_last_update;
        m_last_update = now;

        // Apply simulation speed modifier
        frame_time = Duration(static_cast<std::int64_t>(
            frame_time.count() * m_config.simulation_speed
        ));

        // Prevent spiral of death - cap maximum frame time
        const Duration max_frame_time = m_config.tick_duration() * m_config.max_ticks_per_frame;
        if (frame_time > max_frame_time) {
            frame_time = max_frame_time;
            m_stats.is_lagging = true;
        } else {
            m_stats.is_lagging = false;
        }

        // Accumulate time
        m_accumulator += frame_time;

        // Fixed timestep simulation ticks
        const Duration tick_duration = m_config.tick_duration();
        const double delta_time = m_config.delta_time();
        std::uint32_t ticks_processed = 0;

        const TimePoint tick_start = Clock::now();

        while (m_accumulator >= tick_duration && ticks_processed < m_config.max_ticks_per_frame) {
            // Execute simulation tick
            if (on_tick) {
                on_tick(delta_time, m_stats.total_ticks);
            }

            m_accumulator -= tick_duration;
            ++m_stats.total_ticks;
            ++m_ticks_this_second;
            ++ticks_processed;
        }

        // Measure tick execution time
        const TimePoint tick_end = Clock::now();
        if (ticks_processed > 0) {
            const double total_tick_ms = std::chrono::duration<double, std::milli>(
                tick_end - tick_start
            ).count();
            m_stats.tick_time_ms = total_tick_ms / ticks_processed;
        }

        // Update TPS statistics every second
        update_stats(now);

        // Calculate interpolation alpha for rendering
        // Alpha = how far we are between the last tick and the next tick
        const double alpha = static_cast<double>(m_accumulator.count()) / 
                             static_cast<double>(tick_duration.count());

        m_stats.accumulator_ms = std::chrono::duration<double, std::milli>(m_accumulator).count();

        return alpha;
    }

    // =============================================================================
    // STATISTICS
    // =============================================================================

    [[nodiscard]] const TickStats& stats() const noexcept {
        return m_stats;
    }

    [[nodiscard]] std::uint64_t total_ticks() const noexcept {
        return m_stats.total_ticks;
    }

    [[nodiscard]] std::uint32_t current_tps() const noexcept {
        return m_stats.current_tps;
    }

    [[nodiscard]] double tick_time_ms() const noexcept {
        return m_stats.tick_time_ms;
    }

    // =============================================================================
    // TIME UTILITIES
    // =============================================================================

    // Get current simulation time in seconds
    [[nodiscard]] double simulation_time() const noexcept {
        return m_stats.total_ticks * m_config.delta_time();
    }

    // Get time until next tick in milliseconds
    [[nodiscard]] double time_until_next_tick_ms() const noexcept {
        const Duration remaining = m_config.tick_duration() - m_accumulator;
        return std::chrono::duration<double, std::milli>(remaining).count();
    }

private:
    void update_stats(TimePoint now) {
        const Duration one_second = std::chrono::seconds(1);
        const Duration since_stats_update = now - m_last_stats_update;

        if (since_stats_update >= one_second) {
            m_stats.current_tps = m_ticks_this_second;
            m_stats.ticks_this_second = m_ticks_this_second;
            m_ticks_this_second = 0;
            m_last_stats_update = now;
        }
    }

private:
    TickConfig m_config;
    TickStats m_stats;
    std::atomic<bool> m_running;
    Duration m_accumulator;
    TimePoint m_last_update;
    TimePoint m_last_stats_update;
    std::uint32_t m_ticks_this_second;
};

} // namespace voxel::server
