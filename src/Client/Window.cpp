// =============================================================================
// VOXEL ENGINE - WINDOW WRAPPER IMPLEMENTATION
// =============================================================================

#include "Client/Window.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

// =============================================================================
// OPENGL DEBUG CALLBACK
// =============================================================================
#ifndef NDEBUG
static void APIENTRY gl_debug_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    [[maybe_unused]] GLsizei length,
    const GLchar* message,
    [[maybe_unused]] const void* user_param)
{
    // Ignore non-significant notifications
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    const char* source_str = "Unknown";
    switch (source) {
        case GL_DEBUG_SOURCE_API:             source_str = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_str = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: source_str = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     source_str = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     source_str = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           source_str = "Other"; break;
    }

    const char* type_str = "Unknown";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               type_str = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "Deprecated"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "Undefined Behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         type_str = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              type_str = "Marker"; break;
        case GL_DEBUG_TYPE_OTHER:               type_str = "Other"; break;
    }

    const char* severity_str = "Unknown";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:         severity_str = "HIGH"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       severity_str = "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_LOW:          severity_str = "LOW"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: severity_str = "NOTIFICATION"; break;
    }

    std::cerr << "[GL " << severity_str << "][" << source_str << "][" << type_str << "] "
              << "(ID: " << id << ") " << message << "\n";

    // Break on high severity errors in debug builds
    if (severity == GL_DEBUG_SEVERITY_HIGH && type == GL_DEBUG_TYPE_ERROR) {
        std::cerr << "  ^^^ CRITICAL GL ERROR ^^^\n";
    }
}
#endif

namespace voxel::client {

// =============================================================================
// GLOBAL GLFW STATE
// =============================================================================

static bool s_glfw_initialized = false;
static int s_window_count = 0;

bool initialize_glfw() {
    if (s_glfw_initialized) {
        return true;
    }

    if (!glfwInit()) {
        std::cerr << "[Window] Failed to initialize GLFW\n";
        return false;
    }

    s_glfw_initialized = true;
    return true;
}

void terminate_glfw() {
    if (s_glfw_initialized && s_window_count == 0) {
        glfwTerminate();
        s_glfw_initialized = false;
    }
}

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================

Window::Window() = default;

Window::~Window() {
    destroy();
}

// =============================================================================
// INITIALIZATION
// =============================================================================

bool Window::create(std::int32_t width, std::int32_t height, std::string_view title) {
    if (m_window != nullptr) {
        destroy();
    }

    if (!s_glfw_initialized) {
        if (!initialize_glfw()) {
            return false;
        }
    }

    // OpenGL 4.5 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // Window hints
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    // Debug context in debug builds
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    // Create window
    std::string title_str(title);
    m_window = glfwCreateWindow(width, height, title_str.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[Window] Failed to create GLFW window\n";
        return false;
    }

    m_width = width;
    m_height = height;
    ++s_window_count;

    // Make context current
    glfwMakeContextCurrent(m_window);

    // Load OpenGL functions with glad
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "[Window] Failed to initialize GLAD\n";
        destroy();
        return false;
    }

    // Print OpenGL info
    std::cout << "[Window] OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "[Window] Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "[Window] Vendor: " << glGetString(GL_VENDOR) << "\n";

    // Enable debug output in debug builds
#ifndef NDEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_callback, nullptr);
    std::cout << "[Window] OpenGL debug output enabled\n";
#endif

    // Check for OpenGL 4.5 support
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 4 || (major == 4 && minor < 5)) {
        std::cerr << "[Window] OpenGL 4.5 not supported! Got: " << major << "." << minor << "\n";
        destroy();
        return false;
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);

    // Set callbacks
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSetKeyCallback(m_window, key_callback);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    glfwSetCursorPosCallback(m_window, cursor_pos_callback);
    glfwSetScrollCallback(m_window, scroll_callback);
    glfwSetWindowFocusCallback(m_window, focus_callback);
    glfwSetWindowIconifyCallback(m_window, iconify_callback);

    // Set initial viewport
    glViewport(0, 0, width, height);

    // Default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // VSync on by default
    glfwSwapInterval(1);

    return true;
}

void Window::destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        --s_window_count;

        if (s_window_count == 0) {
            // Don't auto-terminate, let user call terminate_glfw
        }
    }
}

// =============================================================================
// WINDOW LOOP
// =============================================================================

bool Window::should_close() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void Window::set_should_close(bool value) {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, value ? GLFW_TRUE : GLFW_FALSE);
    }
}

void Window::poll_events() {
    // Reset per-frame input
    m_input.reset_deltas();

    // Poll events
    glfwPollEvents();
}

void Window::swap_buffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

// =============================================================================
// INPUT
// =============================================================================

bool Window::is_key_down(std::int32_t key) const {
    if (key >= 0 && key < 512) {
        return m_input.keys[key];
    }
    return false;
}

bool Window::is_key_pressed(std::int32_t key) const {
    if (key >= 0 && key < 512) {
        return m_input.keys_pressed[key];
    }
    return false;
}

bool Window::is_mouse_down(std::int32_t button) const {
    if (button >= 0 && button < 8) {
        return m_input.mouse_buttons[button];
    }
    return false;
}

bool Window::is_mouse_pressed(std::int32_t button) const {
    if (button >= 0 && button < 8) {
        return m_input.mouse_buttons_pressed[button];
    }
    return false;
}

void Window::capture_mouse(bool capture) {
    if (!m_window) return;

    m_input.mouse_captured = capture;
    if (capture) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }

    // Reset first mouse flag to avoid jump
    m_first_mouse = true;
}

// =============================================================================
// VSYNC
// =============================================================================

void Window::set_vsync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
}

// =============================================================================
// TIME
// =============================================================================

double Window::get_time() {
    return glfwGetTime();
}

// =============================================================================
// GLFW CALLBACKS
// =============================================================================

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_width = width;
        self->m_height = height;
        glViewport(0, 0, width, height);
    }
}

void Window::key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && key >= 0 && key < 512) {
        if (action == GLFW_PRESS) {
            self->m_input.keys[key] = true;
            self->m_input.keys_pressed[key] = true;
        } else if (action == GLFW_RELEASE) {
            self->m_input.keys[key] = false;
        }
    }
}

void Window::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && button >= 0 && button < 8) {
        bool pressed = (action == GLFW_PRESS);
        self->m_input.mouse_buttons[button] = pressed;
        if (pressed) {
            self->m_input.mouse_buttons_pressed[button] = true;
        }
    }
}

void Window::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) return;

    if (self->m_first_mouse) {
        self->m_last_mouse_x = xpos;
        self->m_last_mouse_y = ypos;
        self->m_first_mouse = false;
    }

    self->m_input.mouse_dx = xpos - self->m_last_mouse_x;
    self->m_input.mouse_dy = ypos - self->m_last_mouse_y;
    self->m_last_mouse_x = xpos;
    self->m_last_mouse_y = ypos;
    self->m_input.mouse_x = xpos;
    self->m_input.mouse_y = ypos;
}

void Window::scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_input.scroll_y = yoffset;
    }
}

void Window::focus_callback(GLFWwindow* window, int focused) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_focused = (focused == GLFW_TRUE);
        if (!self->m_focused && self->m_input.mouse_captured) {
            // Optionally release mouse on focus loss
            // self->capture_mouse(false);
        }
    }
}

void Window::iconify_callback(GLFWwindow* window, int iconified) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_minimized = (iconified == GLFW_TRUE);
    }
}

} // namespace voxel::client
