// =============================================================================
// VOXEL ENGINE - ENTRY POINT
// Phase 0: Minimal skeleton for build validation
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Server/TickManager.hpp"

#include <cstdio>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    using namespace voxel;

    // Static assertions to validate memory layout
    static_assert(sizeof(Voxel) == 4, "Voxel must be 32-bit POD");
    static_assert(alignof(Chunk) == 64, "Chunk must be 64-byte cache-aligned");
    static_assert(Chunk::VOLUME == 64 * 64 * 64, "Chunk dimensions must be 64^3");

    std::printf("=== VOXEL ENGINE CORE ===\n");
    std::printf("Voxel size:     %zu bytes\n", sizeof(Voxel));
    std::printf("Chunk size:     %zu bytes\n", sizeof(Chunk));
    std::printf("Chunk align:    %zu bytes\n", alignof(Chunk));
    std::printf("Chunk volume:   %u voxels\n", Chunk::VOLUME);
    std::printf("World bounds:   +/- %lld units\n", static_cast<long long>(WORLD_BOUND_MAX));
    std::printf("=========================\n");

    // Phase 0 complete - build system validated
    return 0;
}
