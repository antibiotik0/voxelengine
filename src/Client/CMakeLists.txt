# =============================================================================
# VOXEL ENGINE - CLIENT MODULE (Renderer)
# Contains: OpenGL 4.5 renderer, mesh generation, input handling, window
# Dependencies: Shared + Server modules, OpenGL, GLFW (future)
# =============================================================================

add_library(VoxelClient STATIC
    # Placeholder source - will be populated in future phases
    ${CMAKE_CURRENT_SOURCE_DIR}/client_stub.cpp
)
add_library(Voxel::Client ALIAS VoxelClient)

target_include_directories(VoxelClient PUBLIC
    ${VOXEL_INCLUDE_DIR}
)

target_link_libraries(VoxelClient
    PUBLIC
        Voxel::Shared
        Voxel::Server
    PRIVATE
        voxel_compile_options
)

# Future: OpenGL, GLFW, and other rendering dependencies
# find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)
# target_link_libraries(VoxelClient PRIVATE OpenGL::GL glfw)

# Main executable
add_executable(VoxelEngineApp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_link_libraries(VoxelEngineApp PRIVATE
    Voxel::Client
    voxel_compile_options
)

set_target_properties(VoxelEngineApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
