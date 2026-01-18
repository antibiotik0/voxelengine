// =============================================================================
// VOXEL ENGINE - ENTRY POINT
// Phase 3: World Interaction - Block Breaking/Placing, Collision
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Shared/Settings.hpp"
#include "Shared/Raycast.hpp"
#include "Shared/Collision.hpp"
#include "Server/TickManager.hpp"
#include "Server/World.hpp"
#include "Server/WorldGenerator.hpp"
#include "Client/Window.hpp"
#include "Client/Camera.hpp"
#include "Client/Renderer.hpp"
#include "Client/MeshGenerator.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <optional>

using namespace voxel;
using namespace voxel::server;
using namespace voxel::client;

// =============================================================================
// APPLICATION STATE
// =============================================================================
struct AppState {
    World* world = nullptr;
    Camera camera;
    Renderer renderer;
    MeshGenerator mesh_gen;

    // Timing
    double last_time = 0.0;
    double delta_time = 0.0;
    double fps_time = 0.0;
    int fps_count = 0;
    int current_fps = 0;

    // Camera movement (loaded from settings.toml)
    float move_speed = 10.0f;
    float mouse_sensitivity = 0.15f;
    float player_reach = 5.0f;
    float fov = 70.0f;
    bool first_frame = true;

    // Physics state
    bool collision_enabled = true;
    bool on_ground = false;

    // Debug overlay
    bool show_debug = false;

    // Currently targeted block (from raycast)
    std::optional<RaycastHit> targeted_block;

    // Block type to place (1 = stone by default)
    std::uint16_t selected_block = 1;
};

// =============================================================================
// INPUT PROCESSING
// =============================================================================
void process_input(AppState& app, Window& window) {
    const InputState& input = window.input();
    auto dt = static_cast<float>(app.delta_time);

    // Sprint multiplier
    float speed_mult = 1.0f;
    if (window.is_key_down(GLFW_KEY_LEFT_CONTROL)) {
        speed_mult = 3.0f;
    }

    // Set camera speed (base speed * sprint multiplier)
    app.camera.set_speed(app.move_speed * speed_mult);

    // Process movement - pass delta_time directly, camera uses its own speed
    if (window.is_key_down(GLFW_KEY_W)) {
        app.camera.process_keyboard(Camera::Direction::FORWARD, dt);
    }
    if (window.is_key_down(GLFW_KEY_S)) {
        app.camera.process_keyboard(Camera::Direction::BACKWARD, dt);
    }
    if (window.is_key_down(GLFW_KEY_D)) {
        app.camera.process_keyboard(Camera::Direction::RIGHT, dt);
    }
    if (window.is_key_down(GLFW_KEY_A)) {
        app.camera.process_keyboard(Camera::Direction::LEFT, dt);
    }
    if (window.is_key_down(GLFW_KEY_SPACE)) {
        app.camera.process_keyboard(Camera::Direction::UP, dt);
    }
    if (window.is_key_down(GLFW_KEY_LEFT_SHIFT)) {
        app.camera.process_keyboard(Camera::Direction::DOWN, dt);
    }

    // Mouse look (only when captured)
    // Apply sensitivity here
    if (input.mouse_captured) {
        app.camera.process_mouse(
            static_cast<float>(input.mouse_dx) * app.mouse_sensitivity,
            static_cast<float>(-input.mouse_dy) * app.mouse_sensitivity  // Invert Y for natural mouse look
        );
    }

    // Toggle mouse capture
    if (window.is_key_pressed(GLFW_KEY_ESCAPE)) {
        window.capture_mouse(!input.mouse_captured);
    }

    // Debug overlay toggle (F3)
    if (window.is_key_pressed(GLFW_KEY_F3)) {
        app.show_debug = !app.show_debug;
        std::printf("Debug overlay: %s\n", app.show_debug ? "ON" : "OFF");
    }

    // Toggle collision (F4)
    if (window.is_key_pressed(GLFW_KEY_F4)) {
        app.collision_enabled = !app.collision_enabled;
        std::printf("Collision: %s\n", app.collision_enabled ? "ON" : "OFF");
    }

    // Block selection (1-9 keys)
    for (int i = 0; i < 9; ++i) {
        if (window.is_key_pressed(GLFW_KEY_1 + i)) {
            app.selected_block = static_cast<std::uint16_t>(i + 1);
            std::printf("Selected block: %u\n", app.selected_block);
        }
    }

    // Update camera origin if needed (for origin shifting)
    app.camera.update_origin_if_needed();
}

// =============================================================================
// MESH GENERATION
// =============================================================================
void generate_chunk_meshes(AppState& app, const std::vector<ChunkPosition>& positions) {
    for (const auto& pos : positions) {
        const Chunk* chunk = app.world->get_chunk(pos.x, pos.y, pos.z);
        if (!chunk) continue;

        ChunkMesh mesh;
        app.mesh_gen.generate(*chunk, mesh);

        if (!mesh.is_empty) {
            app.renderer.upload_chunk_mesh(pos, mesh);
        }
    }
}

// =============================================================================
// MAIN
// =============================================================================
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Static assertions to validate memory layout
    static_assert(sizeof(Voxel) == 4, "Voxel must be 32-bit POD");
    static_assert(alignof(Chunk) == 64, "Chunk must be 64-byte cache-aligned");
    static_assert(sizeof(PackedVertex) == 8, "PackedVertex must be 8 bytes");

    std::printf("=== VOXEL ENGINE - PHASE 2 ===\n");
    std::printf("Voxel size:      %zu bytes\n", sizeof(Voxel));
    std::printf("Chunk size:      %zu bytes\n", sizeof(Chunk));
    std::printf("PackedVertex:    %zu bytes\n", sizeof(PackedVertex));
    std::printf("===============================\n\n");

    // Initialize GLFW
    if (!initialize_glfw()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // Create window
    Window window;
    if (!window.create(1280, 720, "Voxel Engine - Phase 2")) {
        std::fprintf(stderr, "Failed to create window\n");
        terminate_glfw();
        return 1;
    }

    // Initialize app state
    AppState app;

    // Load settings from config file
    // Try multiple paths to find settings.toml
    Settings& settings = Settings::instance();
    bool settings_loaded = false;
    
    // Try paths relative to exe location and working directory
    const char* settings_paths[] = {
        "config/settings.toml",
        "../config/settings.toml",
        "../../config/settings.toml",
        "../../../config/settings.toml"
    };
    
    for (const char* path : settings_paths) {
        if (settings.load(path)) {
            settings_loaded = true;
            break;
        }
    }
    
    if (settings_loaded) {
        app.mouse_sensitivity = settings.get_float("input.mouse_sensitivity", 0.15f);
        app.player_reach = settings.get_float("input.player_reach", 5.0f);
        app.fov = settings.get_float("rendering.fov", 70.0f);
        std::printf("[Settings] mouse_sensitivity = %.2f\n", static_cast<double>(app.mouse_sensitivity));
        std::printf("[Settings] player_reach = %.2f\n", static_cast<double>(app.player_reach));
        std::printf("[Settings] fov = %.2f\n", static_cast<double>(app.fov));
    } else {
        std::printf("[Settings] Could not find settings.toml, using defaults\n");
    }

    // Initialize renderer
    if (!app.renderer.initialize()) {
        std::fprintf(stderr, "Failed to initialize renderer\n");
        terminate_glfw();
        return 1;
    }

    // Create world with superflat generator
    WorldConfig world_config;
    world_config.seed = 12345;
    world_config.name = "render_test";

    World world(world_config);
    world.set_generator(generator::create_superflat(SuperflatConfig::classic()));
    app.world = &world;

    std::printf("Generator: %.*s\n",
        static_cast<int>(world.generator()->type_name().size()),
        world.generator()->type_name().data());

    // Load chunks
    std::printf("\n--- Loading Chunks ---\n");
    auto load_start = std::chrono::high_resolution_clock::now();

    constexpr int LOAD_RADIUS = 4;
    std::vector<ChunkPosition> loaded_chunks;

    for (ChunkCoord cx = -LOAD_RADIUS; cx <= LOAD_RADIUS; ++cx) {
        for (ChunkCoord cz = -LOAD_RADIUS; cz <= LOAD_RADIUS; ++cz) {
            if (world.load_chunk(cx, 0, cz)) {
                loaded_chunks.push_back(ChunkPosition{cx, 0, cz});
            }
        }
    }

    auto load_end = std::chrono::high_resolution_clock::now();
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    std::printf("Loaded %zu chunks in %lld ms\n", loaded_chunks.size(), static_cast<long long>(load_time.count()));

    // Generate meshes
    std::printf("\n--- Generating Meshes ---\n");
    auto mesh_start = std::chrono::high_resolution_clock::now();

    generate_chunk_meshes(app, loaded_chunks);

    auto mesh_end = std::chrono::high_resolution_clock::now();
    auto mesh_time = std::chrono::duration_cast<std::chrono::milliseconds>(mesh_end - mesh_start);
    std::printf("Generated %zu chunk meshes in %lld ms\n", 
        app.renderer.uploaded_chunk_count(), 
        static_cast<long long>(mesh_time.count()));
    std::printf("Total vertices: %zu\n", app.renderer.total_vertices());
    std::printf("Total indices:  %zu\n", app.renderer.total_indices());

    // Setup camera - spawn above terrain level
    // Superflat terrain is 8 blocks high (y=0 to y=7), grass is at y=7
    app.camera.set_position(32.0, 12.0, 32.0);  // Center of loaded area, above grass
    app.camera.set_rotation(-30.0f, 45.0f);     // Look down at terrain
    app.camera.set_projection(app.fov, window.aspect_ratio(), 0.1f, 1000.0f);

    // Capture mouse for FPS controls
    window.capture_mouse(true);
    std::printf("Mouse captured - use mouse to look around\n");

    std::printf("\n--- Controls ---\n");
    std::printf("WASD:     Move\n");
    std::printf("Space:    Up\n");
    std::printf("Shift:    Down\n");
    std::printf("Ctrl:     Sprint\n");
    std::printf("Mouse:    Look\n");
    std::printf("LMB:      Break block\n");
    std::printf("RMB:      Place block\n");
    std::printf("1-9:      Select block type\n");
    std::printf("ESC:      Toggle mouse capture\n");
    std::printf("F3:       Debug overlay\n");
    std::printf("F4:       Toggle collision\n");
    std::printf("----------------\n\n");

    // Main loop
    app.last_time = Window::get_time();

    while (!window.should_close()) {
        // Timing
        double current_time = Window::get_time();
        app.delta_time = current_time - app.last_time;
        app.last_time = current_time;

        // FPS counter
        app.fps_count++;
        app.fps_time += app.delta_time;
        if (app.fps_time >= 1.0) {
            app.current_fps = app.fps_count;
            if (!app.show_debug) {
                std::printf("FPS: %d | Draw calls: %zu | Meshes rebuilt: %zu\n",
                    app.fps_count,
                    app.renderer.draw_calls_last_frame(),
                    app.renderer.meshes_rebuilt_last_frame());
            }
            app.fps_count = 0;
            app.fps_time = 0.0;
        }

        // Debug overlay (F3 style) - using int64_t world coordinates
        if (app.show_debug) {
            const auto& pos = app.camera.position();
            
            // Precise world coordinates (int64_t)
            std::int64_t world_x = static_cast<std::int64_t>(std::floor(pos.x));
            std::int64_t world_y = static_cast<std::int64_t>(std::floor(pos.y));
            std::int64_t world_z = static_cast<std::int64_t>(std::floor(pos.z));

            // Chunk coordinates
            ChunkCoord chunk_x = static_cast<ChunkCoord>(world_x >> 6);  // /64 via bit shift
            ChunkCoord chunk_y = static_cast<ChunkCoord>(world_y >> 6);
            ChunkCoord chunk_z = static_cast<ChunkCoord>(world_z >> 6);

            // Local coordinates within chunk
            std::int32_t local_x = static_cast<std::int32_t>(world_x & 63);  // %64 via bit mask
            std::int32_t local_y = static_cast<std::int32_t>(world_y & 63);
            std::int32_t local_z = static_cast<std::int32_t>(world_z & 63);

            std::printf("\033[2J\033[H");  // Clear console (ANSI escape)
            std::printf("=== DEBUG (F3) ===\n");
            std::printf("FPS: %d (%.2f ms/frame)\n", app.current_fps, app.delta_time * 1000.0);
            std::printf("\n--- Position ---\n");
            std::printf("XYZ: %.3f / %.3f / %.3f\n", pos.x, pos.y, pos.z);
            std::printf("Block: %lld %lld %lld\n", 
                static_cast<long long>(world_x),
                static_cast<long long>(world_y),
                static_cast<long long>(world_z));
            std::printf("Chunk: %lld %lld %lld [%d %d %d]\n", 
                static_cast<long long>(chunk_x),
                static_cast<long long>(chunk_y),
                static_cast<long long>(chunk_z),
                local_x, local_y, local_z);
            std::printf("\n--- Rendering ---\n");
            std::printf("Chunks loaded: %zu\n", app.renderer.uploaded_chunk_count());
            std::printf("Draw calls: %zu\n", app.renderer.draw_calls_last_frame());
            std::printf("Vertices: %zu\n", app.renderer.total_vertices());
            std::printf("Meshes rebuilt: %zu\n", app.renderer.meshes_rebuilt_last_frame());
            std::printf("\n--- Target ---\n");
            if (app.targeted_block && app.targeted_block->hit) {
                std::printf("Looking at: %lld, %lld, %lld\n", 
                    static_cast<long long>(app.targeted_block->block_x),
                    static_cast<long long>(app.targeted_block->block_y),
                    static_cast<long long>(app.targeted_block->block_z));
                std::printf("Face: %d, %d, %d\n",
                    app.targeted_block->normal_x,
                    app.targeted_block->normal_y,
                    app.targeted_block->normal_z);
            } else {
                std::printf("Looking at: (none)\n");
            }
            std::printf("\n--- Physics ---\n");
            std::printf("Collision: %s\n", app.collision_enabled ? "ON" : "OFF");
            std::printf("On ground: %s\n", app.on_ground ? "YES" : "NO");
            std::printf("Selected block: %u\n", app.selected_block);
            std::printf("==================\n");
        }

        // Input
        window.poll_events();
        process_input(app, window);

        // Update camera projection if window resized
        app.camera.set_projection(app.fov, window.aspect_ratio(), 0.1f, 1000.0f);

        // Perform block raycast using Amanatides-Woo algorithm
        {
            const auto& cam_pos = app.camera.position();
            const math::Vec3& direction = app.camera.front();

            // Lambda to get voxel from world
            auto get_voxel = [&](std::int64_t x, std::int64_t y, std::int64_t z) -> Voxel {
                return app.world->get_voxel(
                    static_cast<ChunkCoord>(x),
                    static_cast<ChunkCoord>(y),
                    static_cast<ChunkCoord>(z)
                );
            };

            app.targeted_block = VoxelRaycaster::cast(
                cam_pos.x, cam_pos.y, cam_pos.z,
                direction.x, direction.y, direction.z,
                app.player_reach,
                get_voxel
            );
        }

        // Block breaking (Left Mouse Button)
        if (window.is_mouse_pressed(GLFW_MOUSE_BUTTON_LEFT) && 
            window.input().mouse_captured &&
            app.targeted_block && app.targeted_block->hit) {
            
            Voxel broken = app.world->break_block(
                app.targeted_block->block_x,
                app.targeted_block->block_y,
                app.targeted_block->block_z
            );
            
            if (!broken.is_air()) {
                std::printf("Broke block at %lld, %lld, %lld (type=%u)\n",
                    static_cast<long long>(app.targeted_block->block_x),
                    static_cast<long long>(app.targeted_block->block_y),
                    static_cast<long long>(app.targeted_block->block_z),
                    broken.type_id());
            }
        }

        // Block placing (Right Mouse Button)
        if (window.is_mouse_pressed(GLFW_MOUSE_BUTTON_RIGHT) && 
            window.input().mouse_captured &&
            app.targeted_block && app.targeted_block->hit) {
            
            // Place block adjacent to hit face
            std::int64_t place_x = app.targeted_block->block_x + app.targeted_block->normal_x;
            std::int64_t place_y = app.targeted_block->block_y + app.targeted_block->normal_y;
            std::int64_t place_z = app.targeted_block->block_z + app.targeted_block->normal_z;

            Voxel new_block(app.selected_block);
            if (app.world->place_block(place_x, place_y, place_z, new_block)) {
                std::printf("Placed block at %lld, %lld, %lld (type=%u)\n",
                    static_cast<long long>(place_x),
                    static_cast<long long>(place_y),
                    static_cast<long long>(place_z),
                    app.selected_block);
            }
        }

        // Rebuild dirty chunk meshes
        if (app.world->has_dirty_chunks()) {
            auto dirty_positions = app.world->consume_dirty_chunks();
            for (const auto& pos : dirty_positions) {
                const Chunk* chunk = app.world->get_chunk(pos);
                if (chunk) {
                    ChunkMesh mesh;
                    app.mesh_gen.generate(*chunk, mesh);
                    if (!mesh.is_empty) {
                        app.renderer.upload_chunk_mesh(pos, mesh);
                    } else {
                        // Remove empty mesh
                        app.renderer.remove_chunk_mesh(pos);
                    }
                }
            }
        }

        // Render
        app.renderer.begin_frame();
        app.renderer.set_camera(app.camera);
        app.renderer.render_chunks();

        // Render block highlight if targeting a block
        if (app.targeted_block && app.targeted_block->hit) {
            app.renderer.render_block_highlight(
                static_cast<std::int32_t>(app.targeted_block->block_x),
                static_cast<std::int32_t>(app.targeted_block->block_y),
                static_cast<std::int32_t>(app.targeted_block->block_z)
            );
        }

        app.renderer.end_frame();

        window.swap_buffers();
    }

    // Cleanup
    app.renderer.shutdown();
    window.destroy();
    terminate_glfw();

    std::printf("\n=== SHUTDOWN COMPLETE ===\n");
    return 0;
}
