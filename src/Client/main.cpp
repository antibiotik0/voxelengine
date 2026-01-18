// =============================================================================
// VOXEL ENGINE - ENTRY POINT
// Phase 1: World Management Validation
// =============================================================================

#include "Shared/Types.hpp"
#include "Shared/Chunk.hpp"
#include "Server/TickManager.hpp"
#include "Server/World.hpp"
#include "Server/WorldGenerator.hpp"

#include <cstdio>
#include <chrono>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    using namespace voxel;
    using namespace voxel::server;

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
    std::printf("=========================\n\n");

    // Phase 1: World Management Test
    std::printf("=== PHASE 1: WORLD TEST ===\n");

    // Create world with superflat generator
    WorldConfig world_config;
    world_config.seed = 12345;
    world_config.name = "test_world";

    World world(world_config);
    world.set_generator(generator::create_superflat(SuperflatConfig::classic()));

    std::printf("Generator:      %.*s\n", 
                static_cast<int>(world.generator()->type_name().size()),
                world.generator()->type_name().data());

    // Load a 3x3 area of chunks around origin
    auto start = std::chrono::high_resolution_clock::now();

    constexpr int LOAD_RADIUS = 2;
    int chunks_loaded = 0;

    for (ChunkCoord cx = -LOAD_RADIUS; cx <= LOAD_RADIUS; ++cx) {
        for (ChunkCoord cz = -LOAD_RADIUS; cz <= LOAD_RADIUS; ++cz) {
            // Load chunk at Y=0 (contains terrain for superflat)
            if (world.load_chunk(cx, 0, cz)) {
                ++chunks_loaded;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::printf("Chunks loaded:  %d\n", chunks_loaded);
    std::printf("Load time:      %lld us\n", static_cast<long long>(duration.count()));
    std::printf("Per chunk:      %.2f us\n", 
                static_cast<double>(duration.count()) / chunks_loaded);

    // Test voxel access
    std::printf("\n--- Voxel Access Test ---\n");

    // Read voxel at surface (should be grass)
    Voxel surface = world.get_voxel(0, 3, 0); // Y=3 is grass in classic superflat
    std::printf("Surface (0,3,0): type=%u (expected: %u GRASS)\n", 
                surface.type_id(), VoxelType::GRASS);

    // Read voxel below surface (should be dirt)
    Voxel dirt = world.get_voxel(0, 2, 0);
    std::printf("Dirt (0,2,0):    type=%u (expected: %u DIRT)\n", 
                dirt.type_id(), VoxelType::DIRT);

    // Read voxel at bedrock (should be stone)
    Voxel bedrock = world.get_voxel(0, 0, 0);
    std::printf("Bedrock (0,0,0): type=%u (expected: %u STONE)\n", 
                bedrock.type_id(), VoxelType::STONE);

    // Read voxel above surface (should be air)
    Voxel air = world.get_voxel(0, 10, 0);
    std::printf("Air (0,10,0):    type=%u (expected: %u AIR)\n", 
                air.type_id(), VoxelType::AIR);

    // Test voxel modification
    std::printf("\n--- Voxel Modification Test ---\n");

    Voxel stone_block(VoxelType::STONE, 0, 0, 0);
    bool set_result = world.set_voxel(5, 5, 5, stone_block);
    std::printf("Set (5,5,5):     %s\n", set_result ? "SUCCESS" : "FAILED");

    Voxel readback = world.get_voxel(5, 5, 5);
    std::printf("Readback:        type=%u (expected: %u STONE)\n", 
                readback.type_id(), VoxelType::STONE);

    // Bit-shift coordinate test
    std::printf("\n--- Coordinate Transform Test ---\n");

    ChunkCoord test_world = 150; // Should be in chunk 2, local 22
    ChunkCoord chunk_coord = coord::world_to_chunk(test_world);
    LocalCoord local_coord = coord::world_to_local(test_world);
    std::printf("World %lld -> Chunk %lld, Local %d\n", 
                static_cast<long long>(test_world),
                static_cast<long long>(chunk_coord),
                local_coord);

    // Negative coordinate test
    test_world = -100;
    chunk_coord = coord::world_to_chunk(test_world);
    local_coord = coord::world_to_local(test_world);
    std::printf("World %lld -> Chunk %lld, Local %d\n", 
                static_cast<long long>(test_world),
                static_cast<long long>(chunk_coord),
                local_coord);

    std::printf("\nChunk count:    %zu\n", world.chunk_count());
    std::printf("===========================\n");

    return 0;
}
