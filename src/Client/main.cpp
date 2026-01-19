// =============================================================================
// VOXEL ENGINE - ENTRY POINT
// Phase 4: Modular & Advanced Systems
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Shared/Settings.hpp"
#include "Shared/Raycast.hpp"
#include "Shared/Collision.hpp"
#include "Shared/BlockRegistry.hpp"
#include "Server/TickManager.hpp"
#include "Server/World.hpp"
#include "Server/WorldGenerator.hpp"
#include "Server/GeneratorRegistry.hpp"
#include "Server/FluidSimulator.hpp"
#include "Client/Window.hpp"
#include "Client/Camera.hpp"
#include "Client/Renderer.hpp"
#include "Client/MeshGenerator.hpp"
#include "Client/ImGuiDebugOverlay.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

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
    FluidSimulator* fluid_sim = nullptr;  // Phase 4: Fluid simulation
    Camera camera;
    Renderer renderer;
    MeshGenerator mesh_gen;
    ImGuiDebugOverlay debug_overlay;

    // Timing
    double last_time = 0.0;
    double delta_time = 0.0;
    double fps_time = 0.0;
    int fps_count = 0;
    int current_fps = 0;

    // Camera movement (loaded from settings.toml)
    float move_speed = 10.0f;
    float sprint_multiplier = 3.0f;
    float mouse_sensitivity = 0.15f;
    float player_reach = 5.0f;
    float fov = 70.0f;
    bool first_frame = true;

    // Physics state
    bool collision_enabled = true;
    bool on_ground = false;
    
    // Velocity for physics simulation
    double velocity_x = 0.0;
    double velocity_y = 0.0;
    double velocity_z = 0.0;
    
    // Physics constants
    static constexpr double GRAVITY = -28.0;      // Blocks per second squared
    static constexpr double JUMP_VELOCITY = 9.0;  // Blocks per second
    static constexpr double MAX_FALL_SPEED = -50.0;  // Terminal velocity

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

    // Mouse look (only when captured)
    if (input.mouse_captured) {
        app.camera.process_mouse(
            static_cast<float>(input.mouse_dx) * app.mouse_sensitivity,
            static_cast<float>(-input.mouse_dy) * app.mouse_sensitivity
        );
    }

    // Toggle mouse capture
    if (window.is_key_pressed(GLFW_KEY_ESCAPE)) {
        window.capture_mouse(!input.mouse_captured);
    }

    // Debug overlay toggle (F3)
    if (window.is_key_pressed(GLFW_KEY_F3)) {
        app.show_debug = !app.show_debug;
        app.debug_overlay.set_visible(app.show_debug);
    }

    // Toggle collision (F4)
    if (window.is_key_pressed(GLFW_KEY_F4)) {
        app.collision_enabled = !app.collision_enabled;
    }

    // Block selection (1-9 keys)
    for (int i = 0; i < 9; ++i) {
        if (window.is_key_pressed(GLFW_KEY_1 + i)) {
            app.selected_block = static_cast<std::uint16_t>(i + 1);
        }
    }
}

// =============================================================================
// PHYSICS UPDATE
// =============================================================================
void update_physics(AppState& app, Window& window) {
    const double dt = app.delta_time;
    const auto& cam_pos = app.camera.position();
    
    // Get movement input
    double move_x = 0.0, move_z = 0.0;
    
    // Sprint multiplier
    double speed_mult = 1.0;
    if (window.is_key_down(GLFW_KEY_LEFT_CONTROL)) {
        speed_mult = static_cast<double>(app.sprint_multiplier);
    }
    
    const double move_speed = static_cast<double>(app.move_speed) * speed_mult;
    
    // Get horizontal front/right vectors (ignore Y component for ground movement)
    const auto& front = app.camera.front();
    const auto& right = app.camera.right();
    
    // Flatten front vector for horizontal movement
    double front_x = static_cast<double>(front.x);
    double front_z = static_cast<double>(front.z);
    double front_len = std::sqrt(front_x * front_x + front_z * front_z);
    if (front_len > 0.001) {
        front_x /= front_len;
        front_z /= front_len;
    }
    
    double right_x = static_cast<double>(right.x);
    double right_z = static_cast<double>(right.z);
    
    // Accumulate movement direction
    if (window.is_key_down(GLFW_KEY_W)) {
        move_x += front_x;
        move_z += front_z;
    }
    if (window.is_key_down(GLFW_KEY_S)) {
        move_x -= front_x;
        move_z -= front_z;
    }
    if (window.is_key_down(GLFW_KEY_D)) {
        move_x += right_x;
        move_z += right_z;
    }
    if (window.is_key_down(GLFW_KEY_A)) {
        move_x -= right_x;
        move_z -= right_z;
    }
    
    // Normalize if moving diagonally
    double move_len = std::sqrt(move_x * move_x + move_z * move_z);
    if (move_len > 0.001) {
        move_x /= move_len;
        move_z /= move_len;
    }
    
    // Set horizontal velocity from input (instant response for responsive controls)
    app.velocity_x = move_x * move_speed;
    app.velocity_z = move_z * move_speed;
    
    // === COLLISION DISABLED (NOCLIP MODE) ===
    if (!app.collision_enabled) {
        // Free flying - direct position manipulation
        double dx = app.velocity_x * dt;
        double dz = app.velocity_z * dt;
        double dy = 0.0;
        
        // Vertical movement in noclip
        if (window.is_key_down(GLFW_KEY_SPACE)) {
            dy = move_speed * dt;
        }
        if (window.is_key_down(GLFW_KEY_LEFT_SHIFT)) {
            dy = -move_speed * dt;
        }
        
        app.camera.set_position(
            cam_pos.x + dx,
            cam_pos.y + dy,
            cam_pos.z + dz
        );
        app.on_ground = false;
        app.velocity_y = 0.0;
        app.camera.update_origin_if_needed();
        return;
    }
    
    // === COLLISION ENABLED (PHYSICS MODE) ===
    
    // Apply gravity (only when not grounded)
    if (!app.on_ground) {
        app.velocity_y += AppState::GRAVITY * dt;
        // Clamp to terminal velocity
        if (app.velocity_y < AppState::MAX_FALL_SPEED) {
            app.velocity_y = AppState::MAX_FALL_SPEED;
        }
    }
    
    // Jump (only when grounded)
    if (window.is_key_down(GLFW_KEY_SPACE) && app.on_ground) {
        app.velocity_y = AppState::JUMP_VELOCITY;
        app.on_ground = false;
    }
    
    // Calculate position deltas
    double dx = app.velocity_x * dt;
    double dy = app.velocity_y * dt;
    double dz = app.velocity_z * dt;
    
    // Get current position
    double pos_x = cam_pos.x;
    double pos_y = cam_pos.y - CollisionResolver::PLAYER_EYE_HEIGHT; // Convert to feet position
    double pos_z = cam_pos.z;
    
    // Lambda for voxel lookup
    auto get_voxel = [&](std::int64_t bx, std::int64_t by, std::int64_t bz) -> Voxel {
        return app.world->get_voxel(
            static_cast<ChunkCoord>(bx),
            static_cast<ChunkCoord>(by),
            static_cast<ChunkCoord>(bz)
        );
    };
    
    constexpr double HALF_WIDTH = CollisionResolver::PLAYER_WIDTH / 2.0;
    constexpr double HALF_HEIGHT = CollisionResolver::PLAYER_HEIGHT / 2.0;
    
    // Reset on_ground - will be set true if we collide downward
    app.on_ground = false;
    
    // === AXIS-BY-AXIS COLLISION RESOLUTION WITH SNAPPING ===
    
    // --- X AXIS ---
    if (std::abs(dx) > 0.0001) {
        double new_x = pos_x + dx;
        if (CollisionResolver::would_collide(new_x, pos_y, pos_z, HALF_WIDTH, HALF_HEIGHT, get_voxel)) {
            // Snap to block edge
            if (dx > 0) {
                // Moving +X: snap to left edge of blocking voxel
                std::int64_t block_x = static_cast<std::int64_t>(std::floor(new_x + HALF_WIDTH));
                pos_x = static_cast<double>(block_x) - HALF_WIDTH - 0.001;
            } else {
                // Moving -X: snap to right edge of blocking voxel
                std::int64_t block_x = static_cast<std::int64_t>(std::floor(new_x - HALF_WIDTH));
                pos_x = static_cast<double>(block_x + 1) + HALF_WIDTH + 0.001;
            }
            app.velocity_x = 0.0;
        } else {
            pos_x = new_x;
        }
    }
    
    // --- Y AXIS ---
    if (std::abs(dy) > 0.0001) {
        double new_y = pos_y + dy;
        if (CollisionResolver::would_collide(pos_x, new_y, pos_z, HALF_WIDTH, HALF_HEIGHT, get_voxel)) {
            if (dy < 0) {
                // Moving down (falling): snap to top of blocking voxel
                std::int64_t block_y = static_cast<std::int64_t>(std::floor(new_y));
                pos_y = static_cast<double>(block_y + 1) + 0.001;
                app.on_ground = true;
            } else {
                // Moving up (jumping): snap to bottom of blocking voxel
                std::int64_t block_y = static_cast<std::int64_t>(std::floor(new_y + CollisionResolver::PLAYER_HEIGHT));
                pos_y = static_cast<double>(block_y) - CollisionResolver::PLAYER_HEIGHT - 0.001;
            }
            app.velocity_y = 0.0;
        } else {
            pos_y = new_y;
        }
    }
    
    // --- Z AXIS ---
    if (std::abs(dz) > 0.0001) {
        double new_z = pos_z + dz;
        if (CollisionResolver::would_collide(pos_x, pos_y, new_z, HALF_WIDTH, HALF_HEIGHT, get_voxel)) {
            // Snap to block edge
            if (dz > 0) {
                // Moving +Z: snap to near edge of blocking voxel
                std::int64_t block_z = static_cast<std::int64_t>(std::floor(new_z + HALF_WIDTH));
                pos_z = static_cast<double>(block_z) - HALF_WIDTH - 0.001;
            } else {
                // Moving -Z: snap to far edge of blocking voxel
                std::int64_t block_z = static_cast<std::int64_t>(std::floor(new_z - HALF_WIDTH));
                pos_z = static_cast<double>(block_z + 1) + HALF_WIDTH + 0.001;
            }
            app.velocity_z = 0.0;
        } else {
            pos_z = new_z;
        }
    }
    
    // Check if standing on ground (for next frame)
    // Test a small distance below feet
    if (!app.on_ground) {
        if (CollisionResolver::would_collide(pos_x, pos_y - 0.01, pos_z, HALF_WIDTH, HALF_HEIGHT, get_voxel)) {
            app.on_ground = true;
            if (app.velocity_y < 0) {
                app.velocity_y = 0.0;
            }
        }
    }
    
    // Convert back to eye position and update camera
    app.camera.set_position(
        pos_x,
        pos_y + CollisionResolver::PLAYER_EYE_HEIGHT,
        pos_z
    );
    
    app.camera.update_origin_if_needed();
}

// =============================================================================
// MESH GENERATION
// =============================================================================
void generate_chunk_meshes(AppState& app, const std::vector<ChunkPosition>& positions) {
    for (const auto& pos : positions) {
        const Chunk* chunk = app.world->get_chunk(pos.x, pos.y, pos.z);
        if (!chunk) continue;

        // Create neighbor accessor for cross-chunk face culling
        auto neighbor_accessor = [&](ChunkCoord x, ChunkCoord y, ChunkCoord z) -> Voxel {
            return app.world->get_voxel(x, y, z);
        };

        ChunkMesh mesh;
        app.mesh_gen.generate(*chunk, mesh, neighbor_accessor);

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
        app.move_speed = settings.get_float("input.player_speed", 10.0f);
        app.sprint_multiplier = settings.get_float("input.sprint_multiplier", 3.0f);
        app.fov = settings.get_float("rendering.fov", 70.0f);
        std::printf("[Settings] mouse_sensitivity = %.2f\n", static_cast<double>(app.mouse_sensitivity));
        std::printf("[Settings] player_reach = %.2f\n", static_cast<double>(app.player_reach));
        std::printf("[Settings] player_speed = %.2f\n", static_cast<double>(app.move_speed));
        std::printf("[Settings] sprint_multiplier = %.2f\n", static_cast<double>(app.sprint_multiplier));
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
    
    // Initialize BlockRegistry from config FIRST (before loading textures)
    BlockRegistry::instance().load("config/blocks.toml");
    
    // Load block textures into texture array (and resolve block->texture mapping)
    if (!app.renderer.load_textures("assets/textures/blocks")) {
        std::printf("[Warning] Could not load block textures, using default\n");
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init("#version 330");

    app.debug_overlay.init();

    // Create world with superflat generator (using registry for runtime swapping)
    WorldConfig world_config;
    world_config.seed = 12345;
    world_config.name = "render_test";

    World world(world_config);
    
    // Use GeneratorRegistry for swappable generators
    world.set_generator(GeneratorRegistry::instance().create("superflat", world_config.seed));
    app.world = &world;
    
    // Create fluid simulator (Phase 4)
    FluidSimulator fluid_sim(world);
    app.fluid_sim = &fluid_sim;

    std::printf("Generator: %.*s\n",
        static_cast<int>(world.generator()->type_name().size()),
        world.generator()->type_name().data());
    
    // List available generators
    auto generators = GeneratorRegistry::instance().list_generators();
    std::printf("Available generators: ");
    for (const auto& name : generators) {
        std::printf("%s ", name.c_str());
    }
    std::printf("\n");

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
            app.fps_count = 0;
            app.fps_time = 0.0;
        }
        
        // Tick fluid simulation (Phase 4: Modular Fluid Simulation)
        if (app.fluid_sim) {
            app.fluid_sim->tick();
        }

        // Debug overlay (F3 style) - populate data struct for ImGui
        DebugOverlayData debug_data;
        {
            const auto& pos = app.camera.position();
            
            // Precise world coordinates (int64_t)
            debug_data.world_x = static_cast<std::int64_t>(std::floor(pos.x));
            debug_data.world_y = static_cast<std::int64_t>(std::floor(pos.y));
            debug_data.world_z = static_cast<std::int64_t>(std::floor(pos.z));

            // Chunk coordinates
            debug_data.chunk_x = static_cast<std::int32_t>(debug_data.world_x >> 6);  // /64 via bit shift
            debug_data.chunk_y = static_cast<std::int32_t>(debug_data.world_y >> 6);
            debug_data.chunk_z = static_cast<std::int32_t>(debug_data.world_z >> 6);

            // Local coordinates within chunk
            debug_data.local_x = static_cast<std::int32_t>(debug_data.world_x & 63);  // %64 via bit mask
            debug_data.local_y = static_cast<std::int32_t>(debug_data.world_y & 63);
            debug_data.local_z = static_cast<std::int32_t>(debug_data.world_z & 63);
            
            // Performance
            debug_data.fps = static_cast<float>(app.current_fps);
            debug_data.frame_time_ms = static_cast<float>(app.delta_time * 1000.0);
            debug_data.meshes_rebuilt = static_cast<std::uint32_t>(app.renderer.meshes_rebuilt_last_frame());
            debug_data.chunk_count = static_cast<std::uint32_t>(app.renderer.uploaded_chunk_count());
            
            // Player position
            debug_data.player_x = static_cast<float>(pos.x);
            debug_data.player_y = static_cast<float>(pos.y);
            debug_data.player_z = static_cast<float>(pos.z);
            debug_data.velocity_x = static_cast<float>(app.velocity_x);
            debug_data.velocity_y = static_cast<float>(app.velocity_y);
            debug_data.velocity_z = static_cast<float>(app.velocity_z);
            debug_data.on_ground = app.on_ground;
            debug_data.collision_enabled = app.collision_enabled;
            debug_data.selected_block = static_cast<std::uint8_t>(app.selected_block);
            
            // Target block
            if (app.targeted_block && app.targeted_block->hit) {
                debug_data.has_target = true;
                debug_data.target_world_x = app.targeted_block->block_x;
                debug_data.target_world_y = app.targeted_block->block_y;
                debug_data.target_world_z = app.targeted_block->block_z;
                debug_data.target_type = static_cast<std::uint8_t>(app.targeted_block->hit_voxel.type_id());
                debug_data.target_normal_x = app.targeted_block->normal_x;
                debug_data.target_normal_y = app.targeted_block->normal_y;
                debug_data.target_normal_z = app.targeted_block->normal_z;
            } else {
                debug_data.has_target = false;
            }
        }

        // Input
        window.poll_events();
        process_input(app, window);
        
        // Physics update (movement with collision)
        update_physics(app, window);

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
            
            auto bx = app.targeted_block->block_x;
            auto by = app.targeted_block->block_y;
            auto bz = app.targeted_block->block_z;
            
            app.world->break_block(bx, by, bz);
            
            // Notify fluid simulator for potential flow
            if (app.fluid_sim) {
                app.fluid_sim->notify_block_change(bx, by, bz);
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

            // === PLACEMENT SAFETY CHECK ===
            // Check if the new block's AABB would intersect with player's AABB
            const auto& cam_pos = app.camera.position();
            double player_feet_y = cam_pos.y - CollisionResolver::PLAYER_EYE_HEIGHT;
            
            // Player AABB
            AABB player_aabb = AABB::from_center(
                cam_pos.x, 
                player_feet_y + CollisionResolver::PLAYER_HEIGHT / 2.0,
                cam_pos.z,
                CollisionResolver::PLAYER_WIDTH / 2.0,
                CollisionResolver::PLAYER_HEIGHT / 2.0,
                CollisionResolver::PLAYER_WIDTH / 2.0
            );
            
            // Block AABB
            AABB block_aabb = AABB::from_block(place_x, place_y, place_z);
            
            // Only place if no intersection
            if (!player_aabb.intersects(block_aabb)) {
                Voxel new_block(app.selected_block);
                app.world->place_block(place_x, place_y, place_z, new_block);
                
                // Notify fluid simulator
                if (app.fluid_sim) {
                    app.fluid_sim->notify_block_change(place_x, place_y, place_z);
                }
            }
        }

        // Rebuild dirty chunk meshes
        if (app.world->has_dirty_chunks()) {
            auto dirty_positions = app.world->consume_dirty_chunks();
            
            // Create neighbor accessor for cross-chunk face culling
            auto neighbor_accessor = [&](ChunkCoord x, ChunkCoord y, ChunkCoord z) -> Voxel {
                return app.world->get_voxel(x, y, z);
            };
            
            for (const auto& pos : dirty_positions) {
                const Chunk* chunk = app.world->get_chunk(pos);
                if (chunk) {
                    ChunkMesh mesh;
                    app.mesh_gen.generate(*chunk, mesh, neighbor_accessor);
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

        // Render ImGui debug overlay
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        app.debug_overlay.render(debug_data);
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        app.renderer.end_frame();

        window.swap_buffers();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    app.renderer.shutdown();
    window.destroy();
    terminate_glfw();

    std::printf("\n=== SHUTDOWN COMPLETE ===\n");
    return 0;
}
