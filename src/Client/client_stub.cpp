// =============================================================================
// VOXEL ENGINE - CLIENT MODULE STUB
// This file exists to satisfy CMake requirements for STATIC library
// Will be replaced with actual renderer implementation in Phase 2
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"

namespace voxel::client {

// Compilation validation
static_assert(alignof(voxel::Chunk) == 64, "Chunk must be 64-byte aligned");

} // namespace voxel::client
