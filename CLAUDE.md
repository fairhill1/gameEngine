# MyFirstCppGame - 3D Rendering Engine

## Project Overview
A cross-platform 3D game engine built in C++17 using BGFX renderer, SDL3 windowing, and supporting glTF model loading. Successfully refactored from 1378 to 698 lines in main.cpp with zero code duplication.

## Architecture

### Core Systems
- **Rendering**: BGFX with Metal backend on macOS
- **Windowing**: SDL3 for cross-platform window management  
- **3D Models**: TinyGLTF for .glb/.gltf file loading
- **Textures**: STB_Image for PNG/JPEG loading with procedural generation
- **Math**: BX math library for matrices and transformations
- **Terrain**: Procedural chunk-based world generation with biomes

### File Structure
```
src/
├── main.cpp          # Application logic, rendering loop (698 lines)
├── model.h           # Model loading interface
├── model.cpp         # glTF/GLB model processing
assets/               # Textures and 3D models
shaders/metal/        # BGFX Metal shaders for macOS
CMakeLists.txt        # Build configuration
```

## Build System
- **CMake** with dependencies properly linked
- **Dependencies**: SDL3, BGFX, BX, BIMG libraries
- **Platform**: macOS (arm64) with Metal renderer
- **Build**: `make` in project root
- **Run**: `./MyFirstCppGame`

## Current Rendering Objects
1. **Colored Cube** (left) - Vertex color demonstration
2. **Procedural Textured Cube** (right) - Generated sandy texture
3. **PNG Textured Cube** (top) - Loaded high-res texture
4. **Garden Lamp Model** (center) - Working glTF model with proper vertex parsing
5. **Procedural Terrain System** - Infinite world with 4 distinct biomes
6. **Player Cube** - Movable character with terrain following and ray-cast movement

## Unified Rendering Architecture

### Vertex Formats
- **Basic Cubes**: `PosColorVertex` and `PosTexVertex`
- **Model System**: `PosNormalTexcoordVertex` (simplified to Position + TexCoord only)

### Key Functions
- `render_object_at_position()` - Unified rendering for all basic objects
- `Model::render()` - Complex model rendering through unified system
- `load_png_texture()` - Texture loading with fallback support
- `create_procedural_texture()` - Runtime texture generation

### Shader Programs
- **vs_cube.bin + fs_cube.bin** - Colored cube shaders
- **vs_textured_cube.bin + fs_textured_cube.bin** - Textured object shaders (used by both cubes and models)

## Controls
- **WASD** - Camera movement
- **SHIFT + WASD** - Sprint mode
- **Q/E** - Vertical movement
- **Left Mouse + Drag** - Camera rotation
- **Right Mouse Click** - Move player to terrain location (ray casting)
- **ESC** - Exit

## Known Issues
1. **Texture Coordinates**: Complex models may show incorrect textures due to default UV coordinates (0,0) instead of reading actual glTF texture coords
2. **Texture Debugging**: Verbose debug output in model.cpp (can be disabled)

## Development Notes

### BGFX Mouse Ray Casting for Terrain Picking
**CRITICAL**: The correct method for converting mouse coordinates to world space rays in BGFX:

```cpp
Ray createRayFromMouse(float mouseX, float mouseY, int screenWidth, int screenHeight, 
                       const float* viewMatrix, const float* projMatrix) {
    // 1. Convert mouse coordinates to NDC using BGFX method
    float mouseXNDC = (mouseX / (float)screenWidth) * 2.0f - 1.0f;
    float mouseYNDC = ((screenHeight - mouseY) / (float)screenHeight) * 2.0f - 1.0f;
    
    // 2. Create view-projection matrix and its inverse
    float viewProj[16];
    bx::mtxMul(viewProj, viewMatrix, projMatrix);
    float invViewProj[16];
    bx::mtxInverse(invViewProj, viewProj);
    
    // 3. Unproject NDC coordinates to world space using BGFX method
    const bx::Vec3 pickEye = bx::mulH({mouseXNDC, mouseYNDC, 0.0f}, invViewProj);
    const bx::Vec3 pickAt = bx::mulH({mouseXNDC, mouseYNDC, 1.0f}, invViewProj);
    
    // 4. Calculate ray direction
    bx::Vec3 rayDirection = bx::normalize({
        pickAt.x - pickEye.x,
        pickAt.y - pickEye.y,
        pickAt.z - pickEye.z
    });
    
    return {{pickEye.x, pickEye.y, pickEye.z}, {rayDirection.x, rayDirection.y, rayDirection.z}};
}
```

**Key Points**:
- Use **Y-flip** in NDC conversion: `((screenHeight - mouseY) / screenHeight)` 
- Use **bx::mulH()** for homogeneous coordinate multiplication (handles perspective divide)
- Use **inverse view-projection matrix** to unproject from screen to world space
- **Source**: Official BGFX picking example (`examples/30-picking/picking.cpp`)

**Common Mistakes to Avoid**:
- Don't manually calculate ray direction from camera - use unprojection method
- Don't forget Y-flip for screen coordinates 
- Don't use regular matrix multiplication - use `bx::mulH()` for proper perspective divide
- Account for terrain Y-offset when doing intersection tests

## Procedural Terrain & Biome System

### Architecture Overview
The game features an **infinite procedural world** with seamless chunk-based generation supporting 4 distinct biomes. The terrain system provides continuous, smooth landscapes without visible boundaries.

### Core Classes

#### TerrainChunk
```cpp
class TerrainChunk {
    static constexpr int CHUNK_SIZE = 64;           // 64x64 grid per chunk
    static constexpr float SCALE = 0.5f;            // World units per vertex
    static constexpr float HEIGHT_SCALE = 3.0f;     // Vertical scaling factor
    
    BiomeType biome;                                // Chunk's primary biome
    int chunkX, chunkZ;                            // World chunk coordinates
    std::vector<TerrainVertex> vertices;           // 65x65 vertices (overlapping edges)
    std::vector<uint16_t> indices;                 // Triangle indices
    bgfx::VertexBufferHandle vbh, ibh;            // GPU buffers
};
```

#### ChunkManager
```cpp
class ChunkManager {
    static constexpr int RENDER_DISTANCE = 2;      // 5x5 chunk grid around player
    
    std::unordered_map<uint64_t, std::unique_ptr<TerrainChunk>> loadedChunks;
    void updateChunksAroundPlayer(float playerX, float playerZ);
    float getHeightAt(float worldX, float worldZ) const;
    void renderChunks(/* shader params */);
};
```

### Biome Types & Characteristics

1. **Desert**
   - Flat terrain with gentle sand dunes
   - Height variation: 1.2x base + dune features
   - Characteristics: Rolling sandy terrain with fine texture details

2. **Mountains** 
   - High elevation with dramatic steep variations
   - Height variation: 2.5x base + 8.0f elevation + sharp peaks
   - Characteristics: Tallest biome with rocky ridges and sharp details

3. **Swamp**
   - Low-lying, mostly flat marshland  
   - Height variation: 0.4x base - 1.0f (below sea level)
   - Characteristics: Subtle bumps and marshland texture

4. **Grassland**
   - Rolling hills with moderate variation
   - Height variation: 1.5x base + rolling features
   - Characteristics: Gentle rolling terrain, good for settlements

### Seamless World Generation

**Deterministic Procedural Generation**: The current system generates a **consistent, deterministic world**:
- **Same every playthrough**: World coordinates always produce identical terrain/biomes
- **Persistent when revisiting**: Areas regenerate exactly the same when you return  
- **No random seed**: Uses pure mathematical functions based on world position
- **Benefits**: Reliable for testing, exploration, and finding specific areas again
- **Future enhancement**: Could add seed-based variation for different worlds per playthrough

#### Global Noise System
```cpp
float getGlobalNoise(float worldX, float worldZ) const {
    // Multi-octave noise using world coordinates for consistency
    float noise = 0.0f;
    noise += bx::sin(worldX * 0.01f) * bx::cos(worldZ * 0.012f) * 1.0f;      // Large features
    noise += bx::sin(worldX * 0.03f + worldZ * 0.02f) * 0.5f;                // Medium features  
    noise += bx::sin(worldX * 0.08f) * bx::cos(worldZ * 0.075f) * 0.25f;     // Fine detail
    noise += bx::sin(worldX * 0.15f + worldZ * 0.12f) * 0.125f;              // Very fine detail
    return noise * 0.4f;
}
```

#### Biome Assignment
```cpp
BiomeType getBiomeAtWorldPos(float worldX, float worldZ) const {
    // Very large scale noise for big continuous biome regions
    float biomeNoise = bx::sin(worldX * 0.001f) * bx::cos(worldZ * 0.0008f);
    biomeNoise += bx::sin(worldX * 0.0005f + worldZ * 0.0007f) * 0.3f;
    
    if (biomeNoise < -0.3f) return BiomeType::SWAMP;
    if (biomeNoise < -0.1f) return BiomeType::DESERT; 
    if (biomeNoise < 0.2f) return BiomeType::GRASSLAND;
    return BiomeType::MOUNTAINS;
}
```

### Chunk Boundary Stitching

**CRITICAL**: Prevents visible seams between chunks by ensuring shared vertices:

- **Vertex Grid**: Each chunk generates 65x65 vertices (not 64x64)
- **Overlapping Edges**: Adjacent chunks share vertices at their boundaries
- **Consistent Generation**: Same world coordinates always produce identical vertices
- **Index Mapping**: Updated to `vertexRowSize = CHUNK_SIZE + 1` for proper triangle generation

```cpp
// Chunk (0,0) generates vertices from world pos (0,0) to (64,64)
// Chunk (1,0) generates vertices from world pos (64,0) to (128,64)
// Shared edge at X=64 ensures seamless connection
```

### Dynamic Loading System

- **5x5 Chunk Grid**: RENDER_DISTANCE=2 loads chunks around player
- **Memory Management**: Distant chunks automatically unloaded
- **Hash Map Storage**: 64-bit keys for fast chunk lookup: `(chunkX << 32) | chunkZ`
- **Player Tracking**: Chunks load/unload as player moves between chunk boundaries

### Height Sampling & Ray Casting

- **Cross-Chunk Queries**: ChunkManager handles height queries across chunk boundaries
- **Bilinear Interpolation**: Smooth height sampling between vertices
- **Ray-Terrain Intersection**: BGFX-compatible ray casting with terrain offset handling
- **Player Movement**: Right-click to move player with accurate terrain following

### Performance Characteristics

- **Chunk Size**: 64x64 = 4,096 vertices, ~8,000 triangles per chunk
- **Memory**: ~25 chunks loaded simultaneously (5x5 grid)
- **Generation**: Deterministic - same coordinates always produce same terrain
- **Rendering**: Unified rendering pipeline through `render_object_at_position()`

### Key Technical Lessons

1. **World Coordinate Consistency**: All noise functions use world coordinates, not local chunk coordinates
2. **Vertex Overlap**: Extra vertices at chunk edges essential for seamless stitching  
3. **Biome Scale**: Very large wavelengths (0.001f) needed for continuous biome regions
4. **Index Management**: Proper vertex row size calculation critical for GPU buffer correctness

### Critical glTF Parsing Lessons Learned
**IMPORTANT**: When debugging model loading issues:

1. **CRITICAL - Buffer Memory Management**: Always use `bgfx::copy()` for vertex and index buffers, never `bgfx::makeRef()`:
   ```cpp
   // WRONG - causes memory corruption and intermittent failures:
   bgfx::createVertexBuffer(bgfx::makeRef(vertices.data(), size), layout);
   
   // CORRECT - prevents use-after-free bugs:
   const bgfx::Memory* mem = bgfx::copy(vertices.data(), size);
   bgfx::createVertexBuffer(mem, layout);
   ```
   **Symptom**: Model renders correctly "sometimes" but breaks randomly without code changes

2. **Buffer Layout Types**: Check if glTF uses separate or interleaved buffer views:
   - **Separate**: Position, Normal, TexCoord in different buffer views (stride = component size)
   - **Interleaved**: All attributes mixed in same buffer (stride > component size)
   - Use `bufferView.byteStride` to detect: stride=0 means tightly packed separate buffers

3. **Buffer Stride Handling**: For separate buffer views, use component size as stride:
   ```cpp
   // For separate buffer views (byteStride=0):
   size_t stride = 12; // 3 floats for position
   size_t offset = bufferView.byteOffset + accessor.byteOffset + i * stride;
   ```

4. **Vertex Debugging**: If all vertices read as same position, check buffer stride calculation

5. **glTF Component Types**: 
   - componentType=5126 = FLOAT
   - type=3 = VEC3 (position)
   - byteStride=0 means "tightly packed"

6. **Common Symptoms**:
   - **Intermittent loading** = memory corruption (use bgfx::copy())
   - Stretched/deformed models = vertex format mismatch with shaders
   - All vertices same position = buffer stride calculation bug
   - Missing textures = UV coordinate reading issue

### Critical Hash Key Generation for Negative Coordinates

**IMPORTANT**: When using signed coordinates (chunk positions, grid indices) as hash map keys:

```cpp
// WRONG - breaks with negative coordinates:
uint64_t key = (uint64_t(chunkX) << 32) | uint64_t(chunkZ);

// CORRECT - handles negative coordinates properly:
uint64_t getChunkKey(int chunkX, int chunkZ) const {
    uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(chunkX));
    uint64_t z = static_cast<uint64_t>(static_cast<uint32_t>(chunkZ));
    return (x << 32) | z;
}
```

**Symptom**: Objects/chunks with negative coordinates appear "missing" even though debug shows them as "loaded" or "rendered"

**Applies to**: Any coordinate-based systems (entities, spatial indexing, save/load, grid systems)

### Testing Changes
```bash
make && ./MyFirstCppGame
```

### Adding New 3D Objects
1. Use `render_object_at_position()` for simple geometry
2. Use `Model` class for complex glTF models
3. Ensure vertex format matches shader expectations

### Debugging
- Vertex positions printed to stderr for first 5 vertices
- Material and texture loading debug in model.cpp
- BGFX validation enabled in debug builds

## Dependencies Location
- **SDL3**: `sdl_src/SDL-release-3.2.14/`
- **BGFX**: `deps/bgfx/`
- **STB**: `deps/stb/`
- **TinyGLTF**: `deps/tinygltf/`

## Asset Paths
- Textures: `assets/*.png`
- Models: `assets/*.glb` or `assets/*/source/*.glb`
- Shaders: `shaders/metal/*.bin` (copied to build dir)

## Performance
- 60 FPS with VSync on 800x600 window
- Metal renderer with anisotropic filtering
- Efficient vertex formats and unified rendering pipeline
- Zero code duplication - fully refactored architecture

## Future Improvements
1. Fix glTF buffer parsing for complex models
2. Add normal mapping support
3. Implement proper 3D model shaders with lighting
4. Add more primitive shapes to model system
5. Implement biome transition blending at chunk boundaries
6. Add procedural settlement/structure generation in biomes
7. Optimize chunk loading with background threading
8. Add LOD (Level of Detail) system for distant chunks