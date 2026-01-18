// =============================================================================
// VOXEL ENGINE - WINDOW WRAPPER
// GLFW-based window management with OpenGL 4.5 context
// =============================================================================
#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <functional>

// Forward declarations
struct GLFWwindow;

namespace voxel::client {

// =============================================================================
// INPUT STATE
// =============================================================================
struct InputState {
    // Keyboard
    bool keys[512] = {};           // Key states (GLFW key codes)
    bool keys_pressed[512] = {};   // Keys pressed this frame (for single-shot input)

    // Mouse
    double mouse_x = 0.0;
    double mouse_y = 0.0;
    double mouse_dx = 0.0;         // Delta since last frame
    double mouse_dy = 0.0;
    double scroll_y = 0.0;         // Scroll delta
    bool mouse_buttons[8] = {};    // Mouse button states
    bool mouse_captured = false;   // Is mouse captured (hidden cursor)?

    // Reset per-frame deltas
    void reset_deltas() {
        mouse_dx = 0.0;
        mouse_dy = 0.0;
        scroll_y = 0.0;
        for (auto& pressed : keys_pressed) {
            pressed = false;
        }
    }
};

// =============================================================================
// WINDOW CLASS
// =============================================================================
class Window {
public:
    Window();
    ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // =============================================================================
    // INITIALIZATION
    // =============================================================================

    // Create window with OpenGL 4.5 Core context
    bool create(std::int32_t width, std::int32_t height, std::string_view title);

    // Destroy window and cleanup
    void destroy();

    [[nodiscard]] bool is_valid() const noexcept { return m_window != nullptr; }

    // =============================================================================
    // WINDOW LOOP
    // =============================================================================

    // Should window close?
    [[nodiscard]] bool should_close() const;

    // Set window should close
    void set_should_close(bool value);

    // Poll events and update input state
    void poll_events();

    // Swap buffers (present frame)
    void swap_buffers();

    // =============================================================================
    // WINDOW PROPERTIES
    // =============================================================================

    [[nodiscard]] std::int32_t width() const noexcept { return m_width; }
    [[nodiscard]] std::int32_t height() const noexcept { return m_height; }
    [[nodiscard]] float aspect_ratio() const noexcept {
        return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    }
    [[nodiscard]] bool is_focused() const noexcept { return m_focused; }
    [[nodiscard]] bool is_minimized() const noexcept { return m_minimized; }

    // Get GLFW window handle (for advanced usage)
    [[nodiscard]] GLFWwindow* handle() const noexcept { return m_window; }

    // =============================================================================
    // INPUT
    // =============================================================================

    [[nodiscard]] const InputState& input() const noexcept { return m_input; }

    // Check if a key is currently held
    [[nodiscard]] bool is_key_down(std::int32_t key) const;

    // Check if a key was pressed this frame
    [[nodiscard]] bool is_key_pressed(std::int32_t key) const;

    // Check if mouse button is held
    [[nodiscard]] bool is_mouse_down(std::int32_t button) const;

    // Capture/release mouse (hide cursor, enable raw input)
    void capture_mouse(bool capture);

    // =============================================================================
    // VSYNC
    // =============================================================================

    void set_vsync(bool enabled);

    // =============================================================================
    // TIME
    // =============================================================================

    // Get current time since GLFW init (seconds)
    [[nodiscard]] static double get_time();

private:
    // GLFW callbacks
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void focus_callback(GLFWwindow* window, int focused);
    static void iconify_callback(GLFWwindow* window, int iconified);

private:
    GLFWwindow* m_window = nullptr;
    std::int32_t m_width = 0;
    std::int32_t m_height = 0;

    InputState m_input;
    double m_last_mouse_x = 0.0;
    double m_last_mouse_y = 0.0;
    bool m_first_mouse = true;

    bool m_focused = true;
    bool m_minimized = false;
};

// =============================================================================
// GLFW INITIALIZATION (call once at program start)
// =============================================================================
bool initialize_glfw();
void terminate_glfw();

} // namespace voxel::client
