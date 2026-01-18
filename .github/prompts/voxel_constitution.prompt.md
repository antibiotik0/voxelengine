# ARCHITECTURAL SPECIFICATION: VOXEL_ENGINE_CORE (PHASE 0)

## 1. PROJECT PHILOSOPHY
- **Absolute Performance:** Every cycle counts. Prioritize data-oriented design (DOD) and cache locality over deep OOP inheritance.
- **Strict Decoupling:** The "Headless Server" (Simulation) must have zero dependencies on the "Client" (Renderer).
- **Scalability:** The engine must support a world boundary of ±10,000,000 units on the X/Z axes.

## 2. MEMORY ARCHITECTURE (THE VOXEL ATOM)
- **Voxel Representation:** Each voxel is a 32-bit bitfield.
    - [Bits 0-15]: Voxel Type ID (uint16_t).
    - [Bits 16-19]: Sunlight Level (0-15).
    - [Bits 20-23]: Torchlight Level (0-15).
    - [Bits 24-31]: Metadata/Flags (Rotation, fluid-level, or ECS link index).
- **Chunk Geometry:** 64x64x64 (2^18) voxels per chunk. 
- **Storage:** Use flat 1D arrays (`std::unique_ptr<uint32_t[]>`). 
- **Alignment:** Chunks must be 64-byte aligned to match CPU cache lines and support future SIMD/AVX-512 optimization.

## 3. SYSTEMS & SIMULATION
- **TickManager:** Implement a fixed-timestep simulation loop (e.g., 20 TPS). 
- **Temporal Control:** The loop must allow for `simulation_speed` modifiers (0.0 for Freeze, 2.0 for 2x speed) while the Render Loop runs at maximum variable FPS.
- **World Coordinates:** Use `int64_t` for chunk-space coordinates and `int32_t` for local-chunk offsets.
- **Coordinate Transformation:** NO multiplication or division in hot paths. Use bit-shifting only:
    - Index = `(x << 12) | (z << 6) | y` (assuming Y-up).

## 4. RENDERING & GEOMETRY
- **API:** OpenGL 4.5 Core (Approaching Zero Driver Overhead - AZDO).
- **Optimization:** Native Face Culling and Greedy Meshing are mandatory.
- **Precision:** Implement Camera-Relative Rendering (Origin Shifting) to eliminate floating-point jitter at high world coordinates.
- **Vertex Data:** Packed 8-byte structures. Position and UVs must be normalized to fit into `uint16_t` or `uint8_t` where possible.

## 5. EXTENSIBILITY (MODULARITY)
- **Block Entities:** Any voxel requiring logic (inventories, signals) must be handled by an ECS (Entity Component System) separate from the flat voxel array.
- **Configuration:** All engine constants (FOV, Mouse Sensitivity, World Bounds, Gravity) must be loaded from a `settings.toml` via a non-allocating parser.