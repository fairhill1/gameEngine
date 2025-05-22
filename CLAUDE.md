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
4. **Test Square** (bottom) - Model system validation

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
- **Mouse drag** - Camera rotation
- **ESC** - Exit

## Known Issues
1. **Complex Model Loading**: Garden lamp and Spartan models show vertex parsing bugs (all vertices read as same position)
2. **Texture Debugging**: Verbose debug output in model.cpp (can be disabled)

## Development Notes

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