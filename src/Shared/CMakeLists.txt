# =============================================================================
# VOXEL ENGINE - SHARED MODULE
# Header-only library containing core types, interfaces, and utilities
# Zero dependencies on Server or Client modules
# =============================================================================

add_library(VoxelShared INTERFACE)
add_library(Voxel::Shared ALIAS VoxelShared)

target_include_directories(VoxelShared INTERFACE
    ${VOXEL_INCLUDE_DIR}
)

target_link_libraries(VoxelShared INTERFACE
    voxel_compile_options
)

# Ensure C++20 features are available
target_compile_features(VoxelShared INTERFACE cxx_std_20)
