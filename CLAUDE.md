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

### Core Rendering Systems
- **Rendering**: BGFX with Metal backend on macOS
- **Windowing**: SDL3 for cross-platform window management  
- **3D Models**: TinyGLTF for .glb/.gltf file loading via `model.h/cpp`
- **Textures**: STB_Image for PNG/JPEG loading with procedural generation
- **Math**: BX math library for matrices and transformations
- **Terrain**: Procedural chunk-based world generation with biomes (in main.cpp)

### Current File Structure
```
src/
├── main.cpp          # Core game loop, terrain, rendering (1978 lines, down from 2528)
├── model.h/cpp       # glTF/GLB model processing system
├── skills.h/cpp      # Player progression and skill system
├── resources.h/cpp   # Mining and inventory management system  
├── npcs.h/cpp        # AI entity and combat system
├── player.h/cpp      # Player character and movement system
├── camera.h/cpp      # Camera controls and view management
assets/               # Textures and 3D models
shaders/metal/        # BGFX Metal shaders for macOS
CMakeLists.txt        # Build configuration with all modules
```

## Build System
- **CMake** with dependencies properly linked
- **Dependencies**: SDL3, BGFX, BX, BIMG libraries
- **Platform**: macOS (arm64) with Metal renderer
- **Build**: `make` in project root
- **Run**: `./MyFirstCppGame`

## 🏗️ Refactoring History (2025)

### Phase 1: System Extraction
**Goal**: Break monolithic main.cpp into modular, maintainable systems

**Completed Extractions**:
1. ✅ **Skills System** (85 lines) - Player progression with Athletics, Unarmed, and Mining skills
2. ✅ **Resource System** (85 lines) - Mining nodes, inventory management, and resource types  
3. ✅ **NPC System** (200 lines) - AI entities with combat, health, and autonomous behavior
4. ✅ **Player System** (100 lines) - Character movement, combat integration, and health management
5. ✅ **Camera System** (80 lines) - WASD movement, mouse look, and view matrix calculations

**Total Impact**: ~550 lines extracted, main.cpp reduced from 2528 to 1978 lines (-22%)

### Architectural Benefits
- ✅ **Solved Circular Dependencies** - NPCs and Player can now reference each other properly
- ✅ **Clean Separation of Concerns** - Each system has single responsibility
- ✅ **Improved Maintainability** - Easier to modify and extend individual systems
- ✅ **Better Testing** - Systems can be tested in isolation
- ✅ **Reduced Compilation Time** - Only modified systems need recompilation

### Phase 2: Remaining Targets
**Next Extraction Candidates**:
- **Terrain System** (~500 lines) - Largest remaining system with TerrainChunk and ChunkManager
- **Debug System** (~50 lines) - FPS overlay and debug information
- **Rendering Utilities** (~150 lines) - Texture loading and object rendering functions
- **Vertex/Geometry Data** (~200 lines) - Static vertex arrays and primitive definitions
- **Ray Casting System** (~50 lines) - Mouse-to-world coordinate conversion

### Key Lessons Learned
1. **Dependency Management** - Some methods must remain in main.cpp due to ChunkManager dependencies
2. **Header Organization** - Forward declarations essential for breaking circular dependencies  
3. **Interface Design** - Clean public APIs make extraction much easier
4. **Incremental Approach** - Small, focused extractions reduce risk and complexity

## Current Rendering Objects
1. **Colored Cube** (left) - Vertex color demonstration
2. **Procedural Textured Cube** (right) - Generated sandy texture
3. **PNG Textured Cube** (top) - Loaded high-res texture
4. **Garden Lamp Model** (center) - Working glTF model with proper vertex parsing
5. **Procedural Terrain System** - Infinite world with 4 distinct biomes
6. **Player Cube** - Movable character with terrain following and ray-cast movement
7. **Resource Nodes** - Mineable copper (orange), iron (gray), and stone (brown) cubes
8. **Roaming NPCs** - Autonomous AI entities: Wanderers (green), Villagers (blue), Merchants (orange)

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

### Movement & Camera (Camera System)
- **WASD** - Camera movement (extracted to `camera.cpp`)
- **SHIFT + WASD** - Sprint mode (3x speed multiplier)
- **Q/E** - Vertical movement (up/down)
- **Left Mouse + Drag** - Camera rotation with pitch clamping
- **Right Mouse Click** - Move player to terrain location (ray casting)
- **Double Right Click** - Sprint to terrain location
- **1** - Jump to bird's eye view of player

### Game Systems
- **SPACE** - Mine nearby resource nodes (3.0 unit range, integrates with Skills System)
- **I** - Toggle inventory overlay (Resource System)
- **O** - Toggle debug overlay (FPS, chunks, player position)
- **C** - Toggle skills overlay (Skills System - Athletics, Unarmed, Mining)

### Health & Combat Testing
- **H** - Test player damage (20 HP, 1-second immunity)
- **J** - Test player healing (25 HP)
- **K** - Test attack nearby NPCs (3.0 unit range, 25 damage + Unarmed skill modifier)

### System
- **ESC** - Exit game

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

## Procedural Resource Generation System

### Architecture Overview
The game features a **deterministic resource generation system** integrated with chunk loading. Resources spawn when chunks are generated and persist in the world with proper biome-specific distribution.

### Resource Types & Characteristics

1. **Copper** (Orange `0xff4A90E2`)
   - Primary biomes: Desert, Grassland
   - Use: Basic crafting material
   - Health: 100 HP, depletes with 25 damage per mining action

2. **Iron** (Gray `0xff909090`)
   - Primary biomes: Mountains, Swamp, Grassland
   - Use: Advanced crafting material
   - Health: 100 HP, depletes with 25 damage per mining action

3. **Stone** (Brown `0xff808070`)
   - Found in all biomes (lowest priority in most)
   - Use: Building and tool material
   - Health: 100 HP, depletes with 25 damage per mining action

### Biome-Specific Resource Distribution

```cpp
// Biome resource densities and types
MOUNTAINS:   High density (0.5f) - Iron + Stone priority
GRASSLAND:   Balanced (0.35f) - All three resource types
DESERT:      Medium (0.25f) - Copper + Stone priority  
SWAMP:       Low (0.2f) - Iron + Stone only
```

### Key Classes

#### ResourceNode
```cpp
struct ResourceNode {
    bx::Vec3 position;
    ResourceType type;          // COPPER, IRON, STONE
    int health, maxHealth;      // Mining durability
    float size;                 // Render scale
    bool isActive;              // Depletion state
    
    int mine(int damage = 25);  // Returns 1 resource when depleted
    const char* getResourceName() const;
    uint32_t getColor() const;  // Biome-specific colors
};
```

#### PlayerInventory
```cpp
struct PlayerInventory {
    int copper, iron, stone;
    bool showOverlay;
    
    void addResource(ResourceType type, int amount);
    void toggleOverlay();       // I key toggle
    void renderOverlay() const; // BGFX debug text UI
};
```

### Resource Generation Algorithm

**Deterministic Generation**: Uses chunk coordinates + attempt index as seed:
```cpp
float seed = (chunkX * prime1 + chunkZ * prime2 + attempts * prime3);
float noiseValue = bx::sin(seed * freq1) * bx::cos(seed * freq2);

if (noiseValue > (1.0f - density)) {
    // Generate resource node at procedural offset within chunk
    float offsetX = (bx::sin(seed * 0.6f) * 0.5f + 0.5f) * CHUNK_SIZE * SCALE;
    float offsetZ = (bx::cos(seed * 0.7f) * 0.5f + 0.5f) * CHUNK_SIZE * SCALE;
    // Place resource at terrain height
}
```

### Mining System

- **Range**: 3.0 unit radius from player position
- **Input**: SPACE key to mine nearest resource in range
- **Health**: Resources take multiple mining actions to deplete (4 hits at 25 damage each)
- **Feedback**: Console output shows mining progress and resource collection
- **Depletion**: Exhausted nodes disappear from world and stop rendering

### Inventory System

- **UI Overlay**: Toggle with I key, renders in top-left using BGFX debug text
- **Color Coding**: Copper (yellow), Iron (gray), Stone (white) in overlay
- **Real-time Updates**: Shows resource counts as player mines
- **Persistence**: Inventory maintains state throughout gameplay session

## Roaming NPCs System

### Architecture Overview
The game features **autonomous AI entities** that spawn procedurally in biomes and exhibit realistic behavioral patterns including idle states, pathfinding, and terrain-following movement.

### NPC Types & Characteristics

1. **Wanderers** (Green `0xff00AA00`)
   - **Speed**: 2.0 units/second (fastest)
   - **Behavior**: Active roaming with short idle periods (2-5 seconds)
   - **Biomes**: Found in all biomes, common in Mountains/Swamps
   - **Role**: Nomadic travelers, scouts

2. **Villagers** (Blue `0xff0066FF`)
   - **Speed**: 1.0 units/second (slowest)
   - **Behavior**: Longer idle periods (4-8 seconds), deliberate movement
   - **Biomes**: Primarily Grasslands (settlements)
   - **Role**: Local inhabitants, farmers

3. **Merchants** (Orange `0xffFFAA00`)
   - **Speed**: 1.5 units/second (medium)
   - **Behavior**: Balanced idle/movement timing (3-5 seconds)
   - **Biomes**: Grasslands and Desert trade routes
   - **Role**: Trade caravans, economic activity

### AI Behavior System

#### State Machine
```cpp
enum class NPCState {
    IDLE,                    // Standing still, planning next move
    MOVING_TO_TARGET,       // Pathfinding to destination
    WANDERING              // Continuous random walk (unused currently)
};
```

#### Behavior Patterns
- **IDLE → MOVING_TO_TARGET**: Pick random destination 5-15 units away
- **Movement**: Straight-line pathfinding with terrain following
- **Target Selection**: Uses deterministic noise for varied but consistent movement
- **Timeout Protection**: 15-second maximum movement time prevents getting stuck

### Biome-Specific NPC Distribution

```cpp
// NPC spawn densities per biome
GRASSLAND:   Highest (0.15f) - All three NPC types (villages)
DESERT:      Medium (0.08f) - Wanderers + Merchants (trade routes)
MOUNTAINS:   Low (0.05f) - Wanderers only (harsh environment)
SWAMP:       Lowest (0.03f) - Wanderers only (dangerous terrain)
```

### Key Classes

#### NPC
```cpp
struct NPC {
    bx::Vec3 position, velocity, targetPosition;
    NPCType type;               // WANDERER, VILLAGER, MERCHANT
    NPCState state;             // Current AI state
    float speed, size;          // Movement and render properties
    float stateTimer, maxStateTime; // AI timing
    bool isActive;              // Lifecycle management
    uint32_t color;             // Type-specific rendering color
    
    void update(float deltaTime, float terrainHeight);
    const char* getTypeName() const;
};
```

### NPC Generation Algorithm

**Biome-Specific Spawning**: Each biome has custom NPC generation logic:
```cpp
void generateNPCsForChunk(int chunkX, int chunkZ) {
    BiomeType chunkBiome = getBiomeForChunk(chunkX, chunkZ);
    // Different attempt counts and type distributions per biome
    // Deterministic placement using chunk coordinates as seed
    // Terrain height sampling for proper ground placement
}
```

### AI Update System

**Terrain Following**: NPCs automatically adjust Y position to terrain height:
```cpp
npc.update(deltaTime, chunkManager.getHeightAt(npc.position.x, npc.position.z));
```

**Movement Determinism**: Uses position-based seeds for consistent but varied behavior:
```cpp
float angle = (bx::sin(position.x * 0.7f + stateTimer) * 0.5f + 0.5f) * 2.0f * bx::kPi;
float distance = 5.0f + (bx::cos(position.z * 0.8f + stateTimer) * 0.5f + 0.5f) * 10.0f;
```

### Dynamic Entity Management

- **Chunk Integration**: NPCs spawn when chunks load, removed when chunks unload
- **Memory Efficiency**: Only entities in loaded chunks consume memory
- **Persistent Behavior**: Same world coordinates always generate same NPCs
- **Performance**: ~1-3 NPCs per chunk maximum, depending on biome density

## Player Skills System

### Architecture Overview
The game features a **skill-based progression system** that tracks player abilities and provides gameplay modifiers. Skills gain experience through related activities and level up automatically, providing percentage-based bonuses.

### Current Skills

#### Athletics
- **XP Gain**: Movement-based
  - Walking: 0.5 XP per unit traveled
  - Running/Sprinting: 2.0 XP per unit traveled
- **Effect**: +5% movement speed per level (multiplicative)
- **Leveling**: Exponential curve - each level requires 50% more XP than previous

#### Unarmed
- **XP Gain**: Combat-based
  - Successful hit: 5 XP
  - Miss: 1 XP (practice bonus)
- **Effect**: +5% damage per level when fighting without weapons
- **Leveling**: Same exponential curve as Athletics

#### Mining
- **XP Gain**: Resource gathering
  - Each mining action: 2 XP
  - Depleting a node: 10 XP (total)
- **Effect**: +5% mining damage per level (faster resource gathering)
- **Leveling**: Same exponential curve as other skills

### Skills UI
- **Toggle**: Press 'C' to show/hide skills overlay
- **Position**: Bottom-left corner of screen
- **Display Format**:
  ```
  === SKILLS ===
  Athletics Lv.3
    XP: 45/225 (20%)
  Unarmed Lv.2
    XP: 30/150 (20%)
  Mining Lv.1
    XP: 8/100 (8%)
  ===============
  Press C to close
  ```

### Key Classes

#### Skill Structure
```cpp
struct Skill {
    std::string name;
    int level;
    float experience;
    float experienceToNextLevel;
    
    void calculateExpToNextLevel() {
        experienceToNextLevel = 100.0f * bx::pow(1.5f, (float)(level - 1));
    }
    
    float getModifier() const {
        return 1.0f + (level - 1) * 0.05f;  // 5% per level
    }
};
```

#### PlayerSkills Manager
```cpp
struct PlayerSkills {
    std::unordered_map<SkillType, Skill> skills;
    bool showOverlay = false;
    
    void toggleOverlay();       // C key toggle
    void renderOverlay() const; // BGFX debug text UI
    Skill& getSkill(SkillType type);
};
```

### Implementation Details
- **XP Tracking**: Distance-based for Athletics, accumulated until 1 unit traveled
- **Level Up**: Automatic with console notification when XP threshold reached
- **Modifier Application**: Real-time speed and damage calculations
- **Save/Load**: Currently session-based (not persistent between runs)

## Player Health System

### Architecture Overview
The game features a **complete player health system** with visual feedback, damage immunity, and respawn mechanics. Health is permanently displayed at the top-middle of the screen with color-coded status indicators.

### Health Display & UI

#### Health Bar (Always Visible)
- **Location**: Top-middle of screen
- **Format**: "Health: 100/100"
- **Position**: Centered horizontally, top row
- **Color Coding**: 
  - 🟢 **Green** (70%+ health) - Healthy state
  - 🟡 **Yellow** (30%-70% health) - Injured state  
  - 🔴 **Red** (below 30% health) - Critical state

### Health Mechanics

#### Core Stats
```cpp
struct Player {
    int health = 100;           // Current health points
    int maxHealth = 100;        // Maximum health capacity
    float lastDamageTime = 0.0f; // For damage immunity tracking
};
```

#### Damage System
- **Damage Immunity**: 1.0 second cooldown between damage instances
- **Damage Method**: `player.takeDamage(damage, currentTime)`
- **Console Feedback**: Damage taken messages with current health
- **Death Handling**: Automatic respawn when health reaches 0

#### Healing System
- **Healing Method**: `player.heal(amount)`
- **Health Cap**: Cannot exceed maxHealth (100 HP)
- **Console Feedback**: Healing messages with current health
- **Instant Effect**: No cooldown or delay

#### Respawn System
- **Trigger**: Automatic when health ≤ 0
- **Respawn Location**: World origin (0, 0, 0)
- **Health Reset**: Full health restoration (100/100)
- **State Reset**: Clears movement targets and damage immunity

### Key Methods

```cpp
// Damage with immunity checking
void takeDamage(int damage, float currentTime) {
    if (canTakeDamage(currentTime)) {
        health = bx::max(0, health - damage);
        // Handle death and respawn automatically
    }
}

// Healing with health cap
void heal(int amount) {
    health = bx::min(maxHealth, health + amount);
}

// Visual health bar rendering
void renderHealthBar() const {
    // Color-coded health display using BGFX debug text
}
```

## NPC Health System

### Architecture Overview  
NPCs feature **individual health pools** with visual damage indicators, hostility states, and type-specific health values. Health affects NPC behavior and visual appearance through dynamic color changes.

### Health by NPC Type

#### Health Distribution
1. **🟢 Wanderers**: 60 HP
   - **Role**: Medium threat, balanced stats
   - **Behavior**: Becomes hostile when attacked
   - **Combat**: Moderate health pool for extended fights

2. **🔵 Villagers**: 40 HP  
   - **Role**: Peaceful civilians, lowest threat
   - **Behavior**: Never becomes hostile (flees instead)
   - **Combat**: Low health, easy to defeat

3. **🟠 Merchants**: 80 HP
   - **Role**: Well-armed traders, highest threat
   - **Behavior**: Becomes hostile when attacked
   - **Combat**: High health pool, toughest opponents

### Visual Health Indicators

NPCs use **dynamic color changes** to show health status and hostility:

#### Peaceful NPCs (Not Attacked)
- **70%-100% Health**: Full bright base colors
  - Wanderers: Bright green (`0xff00AA00`)
  - Villagers: Bright blue (`0xff0066FF`)
  - Merchants: Bright orange (`0xffFFAA00`)

- **30%-70% Health**: 50% darker base colors
  - Color channels reduced by half for visible damage indication

- **0%-30% Health**: 75% darker base colors  
  - Color channels reduced to 25% for critical damage state

#### Hostile NPCs (When Attacked)
- **All Health Levels**: Red tint override
  - **70%-100% Health**: Dark red (`0xffAA0000`)
  - **30%-70% Health**: Darker red (`0xff880000`)  
  - **0%-30% Health**: Very dark red (`0xff660000`)

#### Dead NPCs
- **0 Health**: Dark gray (`0xff404040`)
- **Inactive State**: Stop rendering and AI updates

### Combat Mechanics

#### Damage System
```cpp
void takeDamage(int damage, float currentTime) {
    health = bx::max(0, health - damage);
    updateHealthColor();        // Visual feedback
    
    // Hostility logic based on NPC type
    if (type != NPCType::VILLAGER) {
        isHostile = true;       // Wanderers/Merchants fight back
    }
    // Villagers remain peaceful (flee instead)
}
```

#### Damage Immunity
- **Cooldown**: 0.5 seconds between damage instances
- **Method**: `canTakeDamage(currentTime)` 
- **Purpose**: Prevents damage spam, allows tactical combat

#### Hostility System
- **Trigger**: Taking damage from player
- **Wanderer Response**: Becomes hostile, aggressive AI
- **Merchant Response**: Becomes hostile, defensive behavior
- **Villager Response**: Remains peaceful, flees from combat

### Key Classes & Methods

#### NPC Health Structure
```cpp
struct NPC {
    int health, maxHealth;      // Health tracking
    bool isHostile;             // Combat state
    float lastDamageTime;       // Damage immunity
    uint32_t color, baseColor;  // Visual health feedback
    
    void takeDamage(int damage, float currentTime);
    void updateHealthColor();   // Dynamic color calculation
    bool canTakeDamage(float currentTime) const;
    void heal(int amount);      // Healing capability
};
```

#### Health Color Algorithm
```cpp
void updateHealthColor() {
    float healthPercent = (float)health / (float)maxHealth;
    
    if (isHostile) {
        // Red tint for hostile NPCs
        color = calculateRedTint(healthPercent);
    } else {
        // Darker base color for peaceful damaged NPCs  
        color = calculateDarkenedColor(baseColor, healthPercent);
    }
}
```

### Combat Integration

#### Testing Framework
- **K Key**: Attack nearby NPCs (3.0 unit range, 25 damage)
- **Range Check**: Distance calculation from player position
- **Target Selection**: Attacks closest NPC in range
- **Feedback**: Console output for damage/death events

#### Combat Flow
1. **Player initiates attack** (K key press)
2. **Range checking** for nearby NPCs (3.0 units)
3. **Damage application** with immunity checking
4. **Visual update** through color changes
5. **Hostility state change** (except villagers)
6. **Death handling** when health reaches 0

### Performance Considerations

- **Color Updates**: Only when health changes, not every frame
- **Immunity Checking**: Lightweight time comparison
- **Visual Changes**: Dynamic color calculation without vertex buffer updates
- **Memory**: Health data adds ~24 bytes per NPC

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

## Recent Additions (2025)

### ✅ Completed Game Features
1. **Procedural Resource Generation System** - Biome-specific copper, iron, and stone nodes
2. **Mining System** - Health-based resource depletion with SPACE key mining
3. **Inventory System** - Visual overlay with I key toggle, real-time resource tracking
4. **Roaming NPCs System** - Autonomous AI entities with biome-specific spawning
5. **AI Behavior States** - Idle, pathfinding, and terrain-following movement
6. **Resource Node Textures** - Proper vertex colors for copper (orange), iron (gray), stone (brown)
7. **Console Output Cleanup** - Removed debug spam from inventory system
8. **Player Health System** - 100 HP with top-screen display, damage immunity, respawn
9. **NPC Health System** - Type-specific health pools (40-80 HP) with visual damage indicators
10. **Combat Framework** - Damage/healing mechanics, hostility states, testing keys (H/J/K)
11. **Player Skills System** - Athletics (movement XP), Unarmed (combat XP), and Mining (resource gathering XP) skills with level progression

### 🏗️ Completed Architecture Refactoring
1. **Skills System Extraction** - `skills.h/cpp` with player progression and XP tracking
2. **Resource System Extraction** - `resources.h/cpp` with mining and inventory management  
3. **NPC System Extraction** - `npcs.h/cpp` with AI entities and combat mechanics
4. **Player System Extraction** - `player.h/cpp` with character movement and health
5. **Camera System Extraction** - `camera.h/cpp` with WASD movement and mouse look
6. **Dependency Resolution** - Solved circular dependencies between Player and NPC systems
7. **Modular Build System** - Updated CMakeLists.txt to compile all extracted modules

### 🎯 Planned Next Features
1. **Terrain System Extraction** - Move TerrainChunk and ChunkManager to `terrain.h/cpp` (~500 lines)
2. **Debug System Extraction** - Move DebugOverlay to `debug.h/cpp` (~50 lines)
3. **Rendering Utilities Extraction** - Move texture and rendering functions (~150 lines)
4. **Crafting System** - Convert resources into tools/items (hotkey: TAB)
5. **Character Stats Screen** - Player progression tracking (hotkey: P)
6. **NPC Interaction** - Trade with merchants, quests from villagers

## Future Improvements
1. Fix glTF buffer parsing for complex models
2. Add normal mapping support
3. Implement proper 3D model shaders with lighting
4. Add more primitive shapes to model system
5. Implement biome transition blending at chunk boundaries
6. Add procedural settlement/structure generation in biomes
7. Optimize chunk loading with background threading
8. Add LOD (Level of Detail) system for distant chunks
9. **NPC Dialogue System** - Text-based conversations with NPCs
10. **Settlement Generation** - Procedural villages in Grassland biomes
11. **Resource Respawning** - Depleted nodes regenerate over time
12. **Improved AI Pathfinding** - Obstacle avoidance and smarter movement