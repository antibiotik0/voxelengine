# VOXEL ENGINE CORE - IMPLEMENTATION ROADMAP

## Phase 0: Project Initialization ✅ COMPLETE
**Objective:** Establish build system, directory structure, and sacred interfaces.

### Deliverables:
- [x] CMakeLists.txt with C++20, strict optimization flags (-O3, /arch:AVX2)
- [x] Modular directory structure (Server, Client, Shared)
- [x] Voxel header: 32-bit POD bitfield
- [x] Chunk header: 64-byte aligned, 64³ storage
- [x] TickManager header: Fixed-timestep simulation loop
- [x] Types header: Coordinate utilities, world constants
- [x] Memory utilities: Aligned allocators

---

## Phase 1: Core Simulation Engine
**Objective:** Implement headless world simulation with chunk management.

### Milestones:
1. **ChunkManager** - Spatial hashing, chunk loading/unloading, LOD system
2. **WorldGenerator** - Procedural terrain (Perlin/Simplex noise)
3. **BlockRegistry** - Runtime block type definitions from config
4. **Lighting Engine** - Sunlight propagation, torchlight BFS
5. **Physics System** - AABB collision, gravity, player movement

### Key Files:
```
include/Server/ChunkManager.hpp
include/Server/WorldGenerator.hpp
include/Server/BlockRegistry.hpp
include/Server/Lighting.hpp
include/Server/Physics.hpp
src/Server/*.cpp
```

### Technical Requirements:
- Zero allocations in hot paths
- Lock-free chunk access for multi-threading
- Spatial partitioning with int64_t coordinates

---

## Phase 2: Rendering Foundation
**Objective:** OpenGL 4.5 AZDO renderer with greedy meshing.

### Milestones:
1. **Window/Context** - GLFW window, OpenGL 4.5 Core context
2. **Shader Pipeline** - Deferred shading, shadow mapping setup
3. **MeshGenerator** - Greedy meshing algorithm, face culling
4. **ChunkRenderer** - Persistent mapped buffers, indirect drawing
5. **Camera System** - Origin shifting for floating-point precision

### Key Files:
```
include/Client/Window.hpp
include/Client/Renderer.hpp
include/Client/MeshGenerator.hpp
include/Client/Camera.hpp
include/Client/Shader.hpp
src/Client/*.cpp
```

### Technical Requirements:
- 8-byte packed vertex format
- Persistent mapped buffers (GL_MAP_PERSISTENT_BIT)
- Camera-relative rendering for precision at ±10M coordinates

---

## Phase 3: World Interaction
**Objective:** Player input, block manipulation, and basic gameplay.

### Milestones:
1. **Input System** - Keyboard/mouse handling, action mapping
2. **Raycast System** - Block selection, placement, breaking
3. **Player Entity** - First-person controller, collision response
4. **Inventory System** - Basic item storage (preparation for ECS)
5. **Serialization** - Chunk save/load (binary format)

### Key Files:
```
include/Client/Input.hpp
include/Shared/Raycast.hpp
include/Server/Player.hpp
include/Shared/Serialization.hpp
```

### Technical Requirements:
- Frame-independent input processing
- Efficient raycast through voxel grid
- Zero-copy serialization where possible

---

## Phase 4: Advanced Systems
**Objective:** ECS integration, fluids, and optimization.

### Milestones:
1. **ECS Foundation** - Entity-Component-System for block entities
2. **Fluid Simulation** - Cellular automata water/lava
3. **Multithreaded Meshing** - Job system for mesh generation
4. **Frustum Culling** - SIMD-accelerated visibility testing
5. **Occlusion Culling** - Hierarchical Z-buffer or software rasterizer

### Key Files:
```
include/Shared/ECS.hpp
include/Server/FluidSimulation.hpp
include/Shared/JobSystem.hpp
include/Client/Culling.hpp
```

### Technical Requirements:
- Lock-free job queue
- SIMD frustum testing (AVX2)
- Fluid updates at reduced tick rate (4 TPS)

---

## Phase 5: Polish & Optimization
**Objective:** Production-ready performance and features.

### Milestones:
1. **Configuration Loader** - TOML parser, hot-reload support
2. **Debug Overlay** - ImGui integration, performance graphs
3. **Audio System** - OpenAL or FMOD integration
4. **Network Foundation** - Client-server protocol (future multiplayer)
5. **Profiling Integration** - Tracy or custom profiler

### Key Files:
```
include/Shared/Config.hpp
include/Client/Debug.hpp
include/Client/Audio.hpp
include/Shared/Network.hpp
```

### Performance Targets:
- 60 FPS minimum at 16-chunk render distance
- < 5ms per tick at 20 TPS
- < 50MB base memory footprint
- < 2ms mesh generation per chunk

---

## Architecture Invariants (Must Never Break)

1. **Server module has ZERO dependencies on Client**
2. **Voxel remains exactly 32 bits**
3. **Chunk remains 64-byte aligned**
4. **All coordinate transforms use bit-shifts only**
5. **No heap allocations in simulation tick**
6. **int64_t for world coordinates, always**

---

## Build Commands

```bash
# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release -j

# Configure with tests
cmake -B build -DVOXEL_BUILD_TESTS=ON

# Run executable
./build/bin/VoxelEngineApp
```

---

*Document Version: 0.1.0 | Last Updated: Phase 0 Completion*
