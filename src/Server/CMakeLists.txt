# =============================================================================
# VOXEL ENGINE - SERVER MODULE (Headless Simulation)
# Contains: World simulation, TickManager, Chunk management, Physics
# Dependencies: Shared module ONLY (no Client dependencies)
# =============================================================================

add_library(VoxelServer STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGenerator.cpp
)
add_library(Voxel::Server ALIAS VoxelServer)

target_include_directories(VoxelServer PUBLIC
    ${VOXEL_INCLUDE_DIR}
)

target_link_libraries(VoxelServer
    PUBLIC
        Voxel::Shared
    PRIVATE
        voxel_compile_options
)

# Enforce no Client dependencies at link time
# Server module must remain headless
set_target_properties(VoxelServer PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
