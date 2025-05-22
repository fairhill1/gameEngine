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
5. **Smooth Terrain** (ground) - Multi-octave noise with bilinear interpolation
6. **Player Cube** - Movable character that follows terrain height

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