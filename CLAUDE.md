# MyFirstCppGame - 3D Rendering Engine

## Project Overview
A cross-platform 3D game engine built in C++17 using BGFX renderer, SDL3 windowing, and supporting glTF model loading. **Successfully refactored from monolithic 2528-line main.cpp into modular architecture with 5 extracted systems (~550 lines removed).**

## Modular Architecture (2025 Refactoring)

### Extracted Systems
- **Skills System**: `skills.h/cpp` - Player progression, XP tracking, and skill modifiers
- **Resource System**: `resources.h/cpp` - Mining nodes, inventory management, and resource types
- **NPC System**: `npcs.h/cpp` - AI entities with combat, movement, and health systems
- **Player System**: `player.h/cpp` - Character movement, combat, skills integration, and health
- **Camera System**: `camera.h/cpp` - WASD movement, mouse look, and view matrix management

### Core Systems (Still in main.cpp)
- **Terrain**: Procedural chunk-based world generation with 4 biomes
- **Rendering**: BGFX with Metal backend, unified rendering pipeline
- **Models**: TinyGLTF for .glb/.gltf file loading via `model.h/cpp`
- **Game Loop**: SDL3 event handling, update/render cycle

### File Structure
```
src/
├── main.cpp          # Core game loop, terrain, rendering (1978 lines, down from 2528)
├── model.h/cpp       # glTF/GLB model processing
├── skills.h/cpp      # Player progression and skill system
├── resources.h/cpp   # Mining and inventory management
├── npcs.h/cpp        # AI entities and combat system
├── player.h/cpp      # Character movement and health
├── camera.h/cpp      # Camera controls and view management
CMakeLists.txt        # Build configuration with all modules
```

## Build System
- **Platform**: macOS (arm64) with Metal renderer
- **Dependencies**: SDL3, BGFX, BX, BIMG libraries
- **Build**: `make` in project root
- **Run**: `./MyFirstCppGame`

## Controls

### Camera & Movement
- **WASD** - Camera movement, **Shift** for sprint, **Q/E** for vertical
- **Left Mouse + Drag** - Camera rotation
- **1** - Jump to bird's eye view of player

### Gameplay
- **Right Click** - Move player to terrain location (double-click to sprint)
- **SPACE** - Mine nearby resource nodes
- **I** - Toggle inventory overlay
- **C** - Toggle skills overlay (Athletics, Unarmed, Mining)
- **O** - Toggle debug overlay
- **H/J/K** - Test damage/healing/combat (development)
- **ESC** - Exit game

## Current Features

### World & Terrain
- **Infinite procedural world** with 4 biomes (Desert, Mountains, Swamp, Grassland)
- **Chunk-based terrain** generation with seamless boundaries
- **Resource nodes** spawning by biome (copper, iron, stone)

### Entities & Combat
- **Player character** with health, movement, and skills progression
- **Roaming NPCs** with AI behavior (Wanderers, Villagers, Merchants)
- **Combat system** with damage, health, and hostility mechanics
- **Mining system** with health-based resource depletion

### Progression
- **Skills system** with XP gain and level progression
  - Athletics (movement), Unarmed (combat), Mining (resource gathering)
- **Inventory system** with resource collection and visual overlay

## 🏗️ Refactoring Progress

### Completed (550 lines extracted)
1. ✅ **Skills System** (85 lines) - Player progression
2. ✅ **Resource System** (85 lines) - Mining and inventory
3. ✅ **NPC System** (200 lines) - AI entities and combat
4. ✅ **Player System** (100 lines) - Character management
5. ✅ **Camera System** (80 lines) - View and input controls

### Benefits Achieved
- ✅ Solved circular dependencies between Player/NPC systems
- ✅ Clean separation of concerns with single responsibility per system
- ✅ Improved maintainability and testing capabilities
- ✅ Reduced main.cpp complexity by 22%

### Next Targets
- **Terrain System** (~500 lines) - TerrainChunk and ChunkManager
- **Debug System** (~50 lines) - FPS overlay and debug info
- **Rendering Utilities** (~150 lines) - Texture loading and helper functions

## Development Notes

### Build Commands
```bash
make                    # Build project
./MyFirstCppGame       # Run game
```

### Key Dependencies
- SDL3 for windowing and input
- BGFX for cross-platform rendering
- TinyGLTF for model loading
- STB for image processing

### Performance
- 60 FPS with VSync on 800x600 window
- Metal renderer with efficient vertex formats
- Chunk-based world loading for infinite terrain

## Architecture Lessons
1. **Incremental refactoring** reduces risk and maintains stability
2. **Forward declarations** essential for breaking circular dependencies
3. **Clean interfaces** make system extraction much easier
4. **Dependency management** - some methods must stay in main.cpp due to heavy coupling

## Future Improvements
1. **Complete modularization** - Extract remaining terrain and utility systems
2. **Enhanced gameplay** - Crafting system, NPC interactions, quests
3. **Technical improvements** - Better model loading, lighting, LOD system
4. **Save/load system** - Persistent world and character progression