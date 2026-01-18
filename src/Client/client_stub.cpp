// =============================================================================
// VOXEL ENGINE - CLIENT MODULE STUB
// This file exists to satisfy CMake requirements for STATIC library
// Will be replaced with actual renderer implementation in Phase 2
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Server/World.hpp"

namespace voxel::client {

// Compilation validation
static_assert(alignof(voxel::Chunk) == 64, "Chunk must be 64-byte aligned");
static_assert(sizeof(voxel::Voxel) == 4, "Voxel must be 32-bit");

} // namespace voxel::client
