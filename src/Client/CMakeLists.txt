# =============================================================================
# VOXEL ENGINE - CLIENT MODULE (Renderer)
# Contains: OpenGL 4.5 renderer, mesh generation, input handling, window
# Dependencies: Shared + Server modules, OpenGL, GLFW, glad
# =============================================================================

add_library(VoxelClient STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
)
add_library(Voxel::Client ALIAS VoxelClient)

target_include_directories(VoxelClient PUBLIC
    ${VOXEL_INCLUDE_DIR}
)

target_link_libraries(VoxelClient
    PUBLIC
        Voxel::Shared
        Voxel::Server
        imgui
        stb_image
    PRIVATE
        voxel_compile_options
        glad
        glfw
)

# Main executable
add_executable(VoxelEngineApp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_link_libraries(VoxelEngineApp PRIVATE
    Voxel::Client
    voxel_compile_options
    glad
)

set_target_properties(VoxelEngineApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Debug single-block test executable
add_executable(VoxelDebugApp
    ${CMAKE_CURRENT_SOURCE_DIR}/main_debug.cpp
)

target_link_libraries(VoxelDebugApp PRIVATE
    Voxel::Client
    voxel_compile_options
    glad
)

set_target_properties(VoxelDebugApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
