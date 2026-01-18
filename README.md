# Voxel Engine Core

High-performance voxel engine built with Data-Oriented Design principles.

## Architecture

```
voxelengine/
├── include/
│   ├── Client/          # Renderer headers
│   ├── Server/          # Simulation headers  
│   └── Shared/          # Core types, utilities
├── src/
│   ├── Client/          # Renderer implementation
│   ├── Server/          # Simulation implementation
│   └── Shared/          # Shared utilities
├── config/              # Runtime configuration
├── assets/              # Textures, shaders, models
└── tests/               # Unit tests
```

## Core Design

- **Voxel:** 32-bit POD bitfield (type, light, metadata)
- **Chunk:** 64³ voxels, 64-byte cache-aligned
- **Coordinates:** int64_t world space, bit-shift transforms
- **Simulation:** Fixed 20 TPS, decoupled from render

## Build

```bash
# Windows (MSVC)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

# Linux/macOS (GCC/Clang)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Requirements

- C++20 compiler (MSVC 19.29+, GCC 11+, Clang 13+)
- CMake 3.25+
- OpenGL 4.5 (future phases)

## License

MIT License - See LICENSE file
