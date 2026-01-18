// =============================================================================
// VOXEL ENGINE - SINGLE BLOCK DEBUG TEST
// Phase 2 Debug: Render ONE block with extensive logging
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Client/Window.hpp"
#include "Client/Camera.hpp"
#include "Client/Renderer.hpp"
#include "Client/PackedVertex.hpp"
#include "Client/ChunkMesh.hpp"
#include "Client/Logger.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>

using namespace voxel;
using namespace voxel::client;

// =============================================================================
// MANUALLY CREATE ONE CUBE MESH
// =============================================================================
ChunkMesh create_single_cube_mesh() {
    ChunkMesh mesh;

    LOG("Mesh", "Creating single cube at position (0, 0, 0)");

    // A single cube at (0,0,0) with all 6 faces
    // Cube extends from (0,0,0) to (1,1,1)

    // Voxel ID for grass (green)
    constexpr std::uint16_t VOXEL_ID = 3; // GRASS
    constexpr std::uint8_t LIGHT = 255;   // Full light
    constexpr std::uint8_t AO = 0;        // No ambient occlusion

    // Helper to create a PackedVertex
    auto make_vertex = [](std::uint32_t x, std::uint32_t y, std::uint32_t z, 
                          std::uint32_t normal_idx, std::uint32_t uv_idx) -> PackedVertex {
        PackedVertex v;
        v.data1 = (x & 0x7F) 
                | ((y & 0x7F) << 7)
                | ((z & 0x7F) << 14)
                | ((normal_idx & 0x7) << 21)
                | ((uv_idx & 0xFF) << 24);
        v.data2 = (VOXEL_ID & 0xFFFF)
                | ((static_cast<std::uint32_t>(LIGHT) & 0xFF) << 16)
                | ((static_cast<std::uint32_t>(AO) & 0xFF) << 24);
        return v;
    };

    // 6 faces, 4 vertices each = 24 vertices
    // 6 faces, 2 triangles each, 6 indices per face = 36 indices

    // Face normals:
    // 0: -X, 1: +X, 2: -Y, 3: +Y, 4: -Z, 5: +Z

    std::uint32_t base = 0;

    // +Y face (top) - normal index 3
    LOG("Mesh", "Adding +Y face (top)");
    mesh.vertices.push_back(make_vertex(0, 1, 0, 3, 0)); // v0
    mesh.vertices.push_back(make_vertex(1, 1, 0, 3, 1)); // v1
    mesh.vertices.push_back(make_vertex(1, 1, 1, 3, 2)); // v2
    mesh.vertices.push_back(make_vertex(0, 1, 1, 3, 3)); // v3
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
    base += 4;

    // -Y face (bottom) - normal index 2
    LOG("Mesh", "Adding -Y face (bottom)");
    mesh.vertices.push_back(make_vertex(0, 0, 0, 2, 0));
    mesh.vertices.push_back(make_vertex(0, 0, 1, 2, 1));
    mesh.vertices.push_back(make_vertex(1, 0, 1, 2, 2));
    mesh.vertices.push_back(make_vertex(1, 0, 0, 2, 3));
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
    base += 4;

    // +X face - normal index 1
    LOG("Mesh", "Adding +X face");
    mesh.vertices.push_back(make_vertex(1, 0, 0, 1, 0));
    mesh.vertices.push_back(make_vertex(1, 0, 1, 1, 1));
    mesh.vertices.push_back(make_vertex(1, 1, 1, 1, 2));
    mesh.vertices.push_back(make_vertex(1, 1, 0, 1, 3));
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
    base += 4;

    // -X face - normal index 0
    LOG("Mesh", "Adding -X face");
    mesh.vertices.push_back(make_vertex(0, 0, 0, 0, 0));
    mesh.vertices.push_back(make_vertex(0, 1, 0, 0, 1));
    mesh.vertices.push_back(make_vertex(0, 1, 1, 0, 2));
    mesh.vertices.push_back(make_vertex(0, 0, 1, 0, 3));
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
    base += 4;

    // +Z face - normal index 5
    LOG("Mesh", "Adding +Z face");
    mesh.vertices.push_back(make_vertex(0, 0, 1, 5, 0));
    mesh.vertices.push_back(make_vertex(0, 1, 1, 5, 1));
    mesh.vertices.push_back(make_vertex(1, 1, 1, 5, 2));
    mesh.vertices.push_back(make_vertex(1, 0, 1, 5, 3));
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
    base += 4;

    // -Z face - normal index 4
    LOG("Mesh", "Adding -Z face");
    mesh.vertices.push_back(make_vertex(0, 0, 0, 4, 0));
    mesh.vertices.push_back(make_vertex(1, 0, 0, 4, 1));
    mesh.vertices.push_back(make_vertex(1, 1, 0, 4, 2));
    mesh.vertices.push_back(make_vertex(0, 1, 0, 4, 3));
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);

    mesh.is_empty = false;

    LOG("Mesh", "Total vertices: ", mesh.vertices.size());
    LOG("Mesh", "Total indices: ", mesh.indices.size());

    // Log all vertices
    LOG_SEP();
    LOG("Mesh", "Vertex data dump:");
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        std::uint32_t x = v.data1 & 0x7F;
        std::uint32_t y = (v.data1 >> 7) & 0x7F;
        std::uint32_t z = (v.data1 >> 14) & 0x7F;
        std::uint32_t normal = (v.data1 >> 21) & 0x7;
        std::uint32_t voxel_id = v.data2 & 0xFFFF;

        LOG("Mesh", "V", i, ": pos=(", x, ",", y, ",", z, ") normal=", normal, " voxelId=", voxel_id, " data1=0x", std::hex, v.data1, std::dec, " data2=0x", std::hex, v.data2, std::dec);
    }

    // Log indices
    LOG_SEP();
    LOG("Mesh", "Index data dump:");
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        LOG("Mesh", "Triangle ", i/3, ": ", mesh.indices[i], ", ", mesh.indices[i+1], ", ", mesh.indices[i+2]);
    }

    return mesh;
}

// =============================================================================
// APPLICATION STATE
// =============================================================================
struct AppState {
    Camera camera;
    Renderer renderer;

    // Timing
    double last_time = 0.0;
    double delta_time = 0.0;
    int fps_count = 0;
    double fps_time = 0.0;

    // Camera movement
    float move_speed = 5.0f;
};

// =============================================================================
// INPUT
// =============================================================================
void process_input(AppState& app, Window& window) {
    const InputState& input = window.input();
    auto dt = static_cast<float>(app.delta_time);

    float speed_mult = window.is_key_down(GLFW_KEY_LEFT_CONTROL) ? 3.0f : 1.0f;
    app.camera.set_speed(app.move_speed * speed_mult);

    if (window.is_key_down(GLFW_KEY_W)) app.camera.process_keyboard(Camera::Direction::FORWARD, dt);
    if (window.is_key_down(GLFW_KEY_S)) app.camera.process_keyboard(Camera::Direction::BACKWARD, dt);
    if (window.is_key_down(GLFW_KEY_D)) app.camera.process_keyboard(Camera::Direction::RIGHT, dt);
    if (window.is_key_down(GLFW_KEY_A)) app.camera.process_keyboard(Camera::Direction::LEFT, dt);
    if (window.is_key_down(GLFW_KEY_SPACE)) app.camera.process_keyboard(Camera::Direction::UP, dt);
    if (window.is_key_down(GLFW_KEY_LEFT_SHIFT)) app.camera.process_keyboard(Camera::Direction::DOWN, dt);

    if (input.mouse_captured) {
        app.camera.process_mouse(static_cast<float>(input.mouse_dx), static_cast<float>(-input.mouse_dy));
    }

    if (window.is_key_pressed(GLFW_KEY_ESCAPE)) {
        window.capture_mouse(!input.mouse_captured);
    }

    // F1 - reserved for future use

    app.camera.update_origin_if_needed();
}

// =============================================================================
// MAIN
// =============================================================================
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Open log file
    if (!Logger::instance().open("voxel_debug.log")) {
        std::fprintf(stderr, "Failed to open log file!\n");
    }

    LOG("Main", "=== VOXEL ENGINE - SINGLE BLOCK DEBUG ===");
    LOG("Main", "PackedVertex size: ", sizeof(PackedVertex), " bytes");

    std::printf("=== SINGLE BLOCK DEBUG TEST ===\n");
    std::printf("Log file: voxel_debug.log\n\n");

    // Initialize GLFW
    if (!initialize_glfw()) {
        LOG("Main", "ERROR: Failed to initialize GLFW");
        return 1;
    }
    LOG("Main", "GLFW initialized");

    // Create window
    Window window;
    if (!window.create(1280, 720, "Voxel Engine - Single Block Test")) {
        LOG("Main", "ERROR: Failed to create window");
        terminate_glfw();
        return 1;
    }
    LOG("Main", "Window created: 1280x720");

    // Log OpenGL info
    LOG("OpenGL", "Version: ", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    LOG("OpenGL", "Renderer: ", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    LOG("OpenGL", "Vendor: ", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    // Initialize app
    AppState app;

    // Initialize renderer
    LOG_SEP();
    LOG("Renderer", "Initializing...");
    if (!app.renderer.initialize()) {
        LOG("Renderer", "ERROR: Failed to initialize");
        terminate_glfw();
        return 1;
    }
    LOG("Renderer", "Initialized successfully");

    // Create single cube mesh
    LOG_SEP();
    LOG("Mesh", "Creating single cube mesh...");
    ChunkMesh cube_mesh = create_single_cube_mesh();

    // Upload to GPU at chunk position (0,0,0)
    LOG_SEP();
    LOG("GPU", "Uploading mesh to GPU...");
    ChunkPosition chunk_pos{0, 0, 0};
    if (!app.renderer.upload_chunk_mesh(chunk_pos, cube_mesh)) {
        LOG("GPU", "ERROR: Failed to upload mesh");
        terminate_glfw();
        return 1;
    }
    LOG("GPU", "Mesh uploaded successfully");
    LOG("GPU", "Total vertices: ", app.renderer.total_vertices());
    LOG("GPU", "Total indices: ", app.renderer.total_indices());

    // Setup camera - position to see the cube at origin
    // Camera at (3, 2, 3) looking towards (0, 0, 0)
    LOG_SEP();
    LOG("Camera", "Setting up camera...");
    app.camera.set_position(3.0, 2.0, 3.0);
    app.camera.set_rotation(-20.0f, -135.0f);  // Look towards origin
    app.camera.set_projection(70.0f, window.aspect_ratio(), 0.1f, 100.0f);

    const auto& pos = app.camera.position();
    LOG("Camera", "Position: (", pos.x, ", ", pos.y, ", ", pos.z, ")");
    LOG("Camera", "Near: 0.1, Far: 100.0, FOV: 70");

    // Log view and projection matrices
    math::Mat4 view = app.camera.view_matrix();
    math::Mat4 proj = app.camera.projection_matrix();
    math::Mat4 vp = app.camera.view_projection_matrix();

    LOG_MAT4("Camera", "View Matrix", view.ptr());
    LOG_MAT4("Camera", "Projection Matrix", proj.ptr());
    LOG_MAT4("Camera", "ViewProjection Matrix", vp.ptr());

    // Test: Transform the cube's vertices through the matrix
    LOG_SEP();
    LOG("Test", "Testing vertex transformation through VP matrix...");
    
    // Test vertex at (0, 1, 0) - top of cube, should be visible
    float test_x = 0.0f, test_y = 1.0f, test_z = 0.0f;
    float clip_x = vp.data[0] * test_x + vp.data[4] * test_y + vp.data[8] * test_z + vp.data[12];
    float clip_y = vp.data[1] * test_x + vp.data[5] * test_y + vp.data[9] * test_z + vp.data[13];
    float clip_z = vp.data[2] * test_x + vp.data[6] * test_y + vp.data[10] * test_z + vp.data[14];
    float clip_w = vp.data[3] * test_x + vp.data[7] * test_y + vp.data[11] * test_z + vp.data[15];

    LOG("Test", "Vertex (0,1,0) -> Clip: (", clip_x, ", ", clip_y, ", ", clip_z, ", ", clip_w, ")");
    if (clip_w != 0) {
        float ndc_x = clip_x / clip_w;
        float ndc_y = clip_y / clip_w;
        float ndc_z = clip_z / clip_w;
        LOG("Test", "Vertex (0,1,0) -> NDC: (", ndc_x, ", ", ndc_y, ", ", ndc_z, ")");
        
        bool in_frustum = (ndc_x >= -1.0f && ndc_x <= 1.0f) &&
                          (ndc_y >= -1.0f && ndc_y <= 1.0f) &&
                          (ndc_z >= 0.0f && ndc_z <= 1.0f);
        LOG("Test", "In frustum: ", in_frustum ? "YES" : "NO");
    }

    // Capture mouse
    window.capture_mouse(true);
    LOG("Main", "Mouse captured");

    std::printf("Camera at (3, 2, 3) looking at cube at origin\n");
    std::printf("Controls: WASD move, Mouse look, F1 wireframe, ESC toggle mouse\n");
    std::printf("Check voxel_debug.log for detailed output\n\n");

    // Main loop
    app.last_time = Window::get_time();
    int frame = 0;

    while (!window.should_close()) {
        // Timing
        double current_time = Window::get_time();
        app.delta_time = current_time - app.last_time;
        app.last_time = current_time;

        // Log first few frames in detail
        if (frame < 3) {
            LOG_SEP();
            LOG("Frame", "=== FRAME ", frame, " ===");
        }

        // FPS
        app.fps_count++;
        app.fps_time += app.delta_time;
        if (app.fps_time >= 1.0) {
            std::printf("FPS: %d | Draw calls: %zu | Vertices: %zu\n",
                app.fps_count, app.renderer.draw_calls_last_frame(), app.renderer.total_vertices());
            app.fps_count = 0;
            app.fps_time = 0.0;
        }

        // Input
        window.poll_events();
        process_input(app, window);

        // Update projection
        app.camera.set_projection(70.0f, window.aspect_ratio(), 0.1f, 100.0f);

        // Render
        app.renderer.begin_frame();

        if (frame < 3) {
            LOG("Frame", "begin_frame called, cleared to sky blue");
        }

        app.renderer.set_camera(app.camera);

        if (frame < 3) {
            const auto& cam_pos = app.camera.position();
            LOG("Frame", "Camera position: (", cam_pos.x, ", ", cam_pos.y, ", ", cam_pos.z, ")");

            math::Mat4 current_vp = app.camera.view_projection_matrix();
            LOG_MAT4("Frame", "Current VP Matrix", current_vp.ptr());
        }

        app.renderer.render_chunks();

        if (frame < 3) {
            LOG("Frame", "render_chunks called, draw calls: ", app.renderer.draw_calls_last_frame());
        }

        app.renderer.end_frame();
        window.swap_buffers();

        if (frame < 3) {
            LOG("Frame", "Frame complete");
        }

        ++frame;
    }

    LOG_SEP();
    LOG("Main", "Shutting down...");

    app.renderer.shutdown();
    window.destroy();
    terminate_glfw();

    LOG("Main", "=== SHUTDOWN COMPLETE ===");
    Logger::instance().close();

    std::printf("\n=== SHUTDOWN COMPLETE ===\n");
    return 0;
}
