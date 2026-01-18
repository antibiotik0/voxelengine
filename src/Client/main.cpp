// =============================================================================
// VOXEL ENGINE - ENTRY POINT
// Phase 2: The Geometry Engine - Full Render Loop
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Server/TickManager.hpp"
#include "Server/World.hpp"
#include "Server/WorldGenerator.hpp"
#include "Client/Window.hpp"
#include "Client/Camera.hpp"
#include "Client/Renderer.hpp"
#include "Client/MeshGenerator.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <chrono>
#include <vector>

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

    // Camera movement
    float move_speed = 10.0f;
    float mouse_sensitivity = 0.1f;
    bool first_frame = true;
};

// =============================================================================
// INPUT PROCESSING
// =============================================================================
void process_input(AppState& app, Window& window) {
    const InputState& input = window.input();
    auto dt = static_cast<float>(app.delta_time);

    // Sprint multiplier
    float speed = app.move_speed;
    if (window.is_key_down(GLFW_KEY_LEFT_CONTROL)) {
        speed *= 3.0f;
    }

    // Process movement
    if (window.is_key_down(GLFW_KEY_W)) {
        app.camera.process_keyboard(Camera::Direction::FORWARD, speed * dt / app.move_speed);
    }
    if (window.is_key_down(GLFW_KEY_S)) {
        app.camera.process_keyboard(Camera::Direction::BACKWARD, speed * dt / app.move_speed);
    }
    if (window.is_key_down(GLFW_KEY_D)) {
        app.camera.process_keyboard(Camera::Direction::RIGHT, speed * dt / app.move_speed);
    }
    if (window.is_key_down(GLFW_KEY_A)) {
        app.camera.process_keyboard(Camera::Direction::LEFT, speed * dt / app.move_speed);
    }
    if (window.is_key_down(GLFW_KEY_SPACE)) {
        app.camera.process_keyboard(Camera::Direction::UP, speed * dt / app.move_speed);
    }
    if (window.is_key_down(GLFW_KEY_LEFT_SHIFT)) {
        app.camera.process_keyboard(Camera::Direction::DOWN, speed * dt / app.move_speed);
    }

    // Mouse look (only when captured)
    if (input.mouse_captured) {
        app.camera.process_mouse(
            static_cast<float>(input.mouse_dx),
            static_cast<float>(input.mouse_dy)
        );
    }

    // Toggle mouse capture
    if (window.is_key_pressed(GLFW_KEY_ESCAPE)) {
        window.capture_mouse(!input.mouse_captured);
    }

    // Wireframe toggle
    if (window.is_key_pressed(GLFW_KEY_F1)) {
        static bool wireframe = false;
        wireframe = !wireframe;
        app.renderer.set_wireframe(wireframe);
        std::printf("Wireframe: %s\n", wireframe ? "ON" : "OFF");
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

    // Setup camera
    app.camera.set_position(0.0, 20.0, 30.0);  // Start above and behind
    app.camera.set_rotation(-20.0f, -90.0f);   // Look down and forward
    app.camera.set_projection(70.0f, window.aspect_ratio(), 0.1f, 1000.0f);

    // Capture mouse
    window.capture_mouse(true);

    std::printf("\n--- Controls ---\n");
    std::printf("WASD:     Move\n");
    std::printf("Space:    Up\n");
    std::printf("Shift:    Down\n");
    std::printf("Ctrl:     Sprint\n");
    std::printf("Mouse:    Look\n");
    std::printf("ESC:      Toggle mouse capture\n");
    std::printf("F1:       Toggle wireframe\n");
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
            std::printf("FPS: %d | Draw calls: %zu | Vertices: %zu\n",
                app.fps_count,
                app.renderer.draw_calls_last_frame(),
                app.renderer.total_vertices());
            app.fps_count = 0;
            app.fps_time = 0.0;
        }

        // Input
        window.poll_events();
        process_input(app, window);

        // Update camera projection if window resized
        app.camera.set_projection(70.0f, window.aspect_ratio(), 0.1f, 1000.0f);

        // Render
        app.renderer.begin_frame();
        app.renderer.set_camera(app.camera);
        app.renderer.render_chunks();
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
