# =============================================================================
# VOXEL ENGINE CORE - ROOT BUILD CONFIGURATION
# Phase 0: Project Initialization
# Target: C++20 | AZDO OpenGL 4.5 | Data-Oriented Design
# =============================================================================
cmake_minimum_required(VERSION 3.25)

project(VoxelEngine
    VERSION 0.1.0
    DESCRIPTION "High-Performance Voxel Engine with DOD Architecture"
    LANGUAGES C CXX
)

# =============================================================================
# GLOBAL C++ STANDARD ENFORCEMENT
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# BUILD TYPE DEFAULT
# =============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# =============================================================================
# COMPILER-SPECIFIC OPTIMIZATION FLAGS
# =============================================================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC: Maximum optimization + AVX2 SIMD
    set(VOXEL_COMPILE_FLAGS
        /W4                     # High warning level
        /WX                     # Warnings as errors
        /permissive-            # Strict conformance
        /Zc:__cplusplus         # Correct __cplusplus macro
        /Zc:preprocessor        # Standard-conforming preprocessor
        /fp:fast                # Fast floating-point
        /GR-                    # Disable RTTI (performance)
        /EHsc                   # Exception handling
    )
    set(VOXEL_RELEASE_FLAGS
        /O2                     # Maximum optimization
        /Ob3                    # Aggressive inlining
        /arch:AVX2              # AVX2 SIMD instructions
        /GL                     # Whole program optimization
        /Oi                     # Enable intrinsics
        /Ot                     # Favor fast code
        /DNDEBUG
    )
    set(VOXEL_LINK_FLAGS_RELEASE
        /LTCG                   # Link-time code generation
        /OPT:REF                # Remove unreferenced code
        /OPT:ICF                # Identical COMDAT folding
    )
    set(VOXEL_DEBUG_FLAGS
        /Od                     # No optimization
        /Zi                     # Debug information
        /RTC1                   # Runtime checks
        /D_DEBUG
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang: Maximum optimization + native architecture
    set(VOXEL_COMPILE_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wshadow
        -Wcast-align
        -Wunused
        -Wnull-dereference
        -Wdouble-promotion
        -fno-rtti                # Disable RTTI (performance)
        -fno-exceptions          # Disable exceptions (can be enabled per-module)
    )
    set(VOXEL_RELEASE_FLAGS
        -O3                      # Maximum optimization
        -march=native            # Native CPU architecture
        -mtune=native            # Native CPU tuning
        -ffast-math              # Fast floating-point
        -flto                    # Link-time optimization
        -fomit-frame-pointer     # Free up frame pointer register
        -funroll-loops           # Loop unrolling
        -ftree-vectorize         # Auto-vectorization
        -DNDEBUG
    )
    set(VOXEL_LINK_FLAGS_RELEASE
        -flto
        -s                       # Strip symbols
    )
    set(VOXEL_DEBUG_FLAGS
        -O0
        -g3
        -fno-omit-frame-pointer
        -D_DEBUG
    )
    set(VOXEL_DEBUG_LINK_FLAGS
        # Sanitizers disabled on Windows/MinGW
    )
endif()

# =============================================================================
# APPLY FLAGS VIA INTERFACE LIBRARY
# =============================================================================
add_library(voxel_compile_options INTERFACE)
target_compile_options(voxel_compile_options INTERFACE ${VOXEL_COMPILE_FLAGS})

target_compile_options(voxel_compile_options INTERFACE
    $<$<CONFIG:Release>:${VOXEL_RELEASE_FLAGS}>
    $<$<CONFIG:RelWithDebInfo>:${VOXEL_RELEASE_FLAGS}>
    $<$<CONFIG:Debug>:${VOXEL_DEBUG_FLAGS}>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(voxel_compile_options INTERFACE
        $<$<CONFIG:Release>:${VOXEL_LINK_FLAGS_RELEASE}>
        $<$<CONFIG:Debug>:${VOXEL_DEBUG_LINK_FLAGS}>
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_options(voxel_compile_options INTERFACE
        $<$<CONFIG:Release>:${VOXEL_LINK_FLAGS_RELEASE}>
    )
endif()

# =============================================================================
# INCLUDE DIRECTORIES
# =============================================================================
set(VOXEL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# =============================================================================
# EXTERNAL DEPENDENCIES
# =============================================================================
include(FetchContent)

# GLFW - Window and Input
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# glad - OpenGL Loader (local)
add_subdirectory(external/glad)

# =============================================================================
# SUBDIRECTORIES (MODULAR ARCHITECTURE)
# =============================================================================
add_subdirectory(src/Shared)
add_subdirectory(src/Server)
add_subdirectory(src/Client)

# =============================================================================
# OPTIONAL: TESTING
# =============================================================================
option(VOXEL_BUILD_TESTS "Build unit tests" OFF)
if(VOXEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# INSTALLATION (Future Phase)
# =============================================================================
# install(TARGETS ...)
