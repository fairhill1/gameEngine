#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bimg/decode.h>
#include <bx/math.h>
#include <bx/file.h>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cfloat>
#include <random>
#include <unordered_map>
#include <string>
#include <memory>

// Define STB_IMAGE_IMPLEMENTATION before including to create the implementation
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Define TINYGLTF_IMPLEMENTATION and required flags once in the project
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"
#include "json.hpp"

// Provide stub implementations for TinyGLTF image functions since we disabled STB_IMAGE in TinyGLTF
namespace tinygltf {
    bool LoadImageData(Image* image, const int image_idx, std::string* err, std::string* warn,
                      int req_width, int req_height, const unsigned char* bytes, int size, void* user_data) {
        // This is handled by our custom image loader in the model loading code
        return false; // Return false to indicate we're not handling it here
    }
    
    bool WriteImageData(const std::string* basepath, const std::string* filename,
                       const Image* image, bool embedImages, const FsCallbacks* fs, 
                       const URICallbacks* uri_cb, std::string* out_uri, void* user_data) {
        // We don't support writing images
        return false;
    }
}

// Include our model system
#include "model.h"
#include "skills.h"
#include "resources.h"
#include "npcs.h"
#include "player.h"
#include "camera.h"
#include "ui.h"

// Window dimensions
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const char* WINDOW_TITLE = "My First C++ Game with BGFX";

// Colors
const uint32_t CLEAR_COLOR = 0x303030ff;


// Debug overlay system
struct DebugOverlay {
    bool enabled;
    float fps;
    float frameTime;
    Uint64 lastTime;
    Uint32 frameCount;
    
    DebugOverlay() : enabled(false), fps(0.0f), frameTime(0.0f), lastTime(0), frameCount(0) {}
    
    void update() {
        frameCount++;
        Uint64 currentTime = SDL_GetPerformanceCounter();
        
        if (lastTime == 0) {
            lastTime = currentTime;
            return;
        }
        
        Uint64 frequency = SDL_GetPerformanceFrequency();
        float deltaTime = (float)(currentTime - lastTime) / frequency;
        
        // Update FPS every 0.25 seconds
        if (deltaTime >= 0.25f) {
            fps = frameCount / deltaTime;
            frameTime = deltaTime * 1000.0f / frameCount;
            frameCount = 0;
            lastTime = currentTime;
        }
    }
    
    void toggle() {
        enabled = !enabled;
        std::cout << "Debug overlay " << (enabled ? "enabled" : "disabled") << std::endl;
    }
};

// Vertex structures for basic primitives
struct PosColorVertex {
    float x, y, z;
    uint32_t abgr;
};

struct PosTexVertex {
    float x, y, z;
    float u, v;
};

// Colored cube vertices
static PosColorVertex cubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff0000ff}, // 0: Front-top-left (red)
    { 1.0f,  1.0f,  1.0f, 0xff00ff00}, // 1: Front-top-right (green)
    {-1.0f, -1.0f,  1.0f, 0xff0000ff}, // 2: Front-bottom-left (red)
    { 1.0f, -1.0f,  1.0f, 0xff00ff00}, // 3: Front-bottom-right (green)
    {-1.0f,  1.0f, -1.0f, 0xffff0000}, // 4: Back-top-left (blue)
    { 1.0f,  1.0f, -1.0f, 0xffffff00}, // 5: Back-top-right (yellow)
    {-1.0f, -1.0f, -1.0f, 0xffff0000}, // 6: Back-bottom-left (blue)
    { 1.0f, -1.0f, -1.0f, 0xffffff00}  // 7: Back-bottom-right (yellow)
};

// Copper-colored cube vertices (orange-brown copper color)
static PosColorVertex copperCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff4A90E2}, // 0: Front-top-left (copper orange)
    { 1.0f,  1.0f,  1.0f, 0xff4A90E2}, // 1: Front-top-right (copper orange)
    {-1.0f, -1.0f,  1.0f, 0xff3A7AC2}, // 2: Front-bottom-left (darker copper)
    { 1.0f, -1.0f,  1.0f, 0xff3A7AC2}, // 3: Front-bottom-right (darker copper)
    {-1.0f,  1.0f, -1.0f, 0xff5AA0F2}, // 4: Back-top-left (lighter copper)
    { 1.0f,  1.0f, -1.0f, 0xff5AA0F2}, // 5: Back-top-right (lighter copper)
    {-1.0f, -1.0f, -1.0f, 0xff3A7AC2}, // 6: Back-bottom-left (darker copper)
    { 1.0f, -1.0f, -1.0f, 0xff3A7AC2}  // 7: Back-bottom-right (darker copper)
};

// Iron-colored cube vertices (metallic gray)
static PosColorVertex ironCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff909090}, // 0: Front-top-left (light gray)
    { 1.0f,  1.0f,  1.0f, 0xff909090}, // 1: Front-top-right (light gray)
    {-1.0f, -1.0f,  1.0f, 0xff606060}, // 2: Front-bottom-left (darker gray)
    { 1.0f, -1.0f,  1.0f, 0xff606060}, // 3: Front-bottom-right (darker gray)
    {-1.0f,  1.0f, -1.0f, 0xffA0A0A0}, // 4: Back-top-left (lighter gray)
    { 1.0f,  1.0f, -1.0f, 0xffA0A0A0}, // 5: Back-top-right (lighter gray)
    {-1.0f, -1.0f, -1.0f, 0xff606060}, // 6: Back-bottom-left (darker gray)
    { 1.0f, -1.0f, -1.0f, 0xff606060}  // 7: Back-bottom-right (darker gray)
};

// Stone-colored cube vertices (rocky gray-brown)
static PosColorVertex stoneCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff808070}, // 0: Front-top-left (gray-brown)
    { 1.0f,  1.0f,  1.0f, 0xff808070}, // 1: Front-top-right (gray-brown)
    {-1.0f, -1.0f,  1.0f, 0xff504540}, // 2: Front-bottom-left (darker stone)
    { 1.0f, -1.0f,  1.0f, 0xff504540}, // 3: Front-bottom-right (darker stone)
    {-1.0f,  1.0f, -1.0f, 0xff909080}, // 4: Back-top-left (lighter stone)
    { 1.0f,  1.0f, -1.0f, 0xff909080}, // 5: Back-top-right (lighter stone)
    {-1.0f, -1.0f, -1.0f, 0xff504540}, // 6: Back-bottom-left (darker stone)
    { 1.0f, -1.0f, -1.0f, 0xff504540}  // 7: Back-bottom-right (darker stone)
};

// Wanderer NPC cube vertices (green)
static PosColorVertex wandererCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff00AA00}, // 0: Front-top-left (green)
    { 1.0f,  1.0f,  1.0f, 0xff00AA00}, // 1: Front-top-right
    {-1.0f, -1.0f,  1.0f, 0xff008800}, // 2: Front-bottom-left (darker green)
    { 1.0f, -1.0f,  1.0f, 0xff008800}, // 3: Front-bottom-right (darker green)
    {-1.0f,  1.0f, -1.0f, 0xff00CC00}, // 4: Back-top-left (lighter green)
    { 1.0f,  1.0f, -1.0f, 0xff00CC00}, // 5: Back-top-right (lighter green)
    {-1.0f, -1.0f, -1.0f, 0xff008800}, // 6: Back-bottom-left (darker green)
    { 1.0f, -1.0f, -1.0f, 0xff008800}  // 7: Back-bottom-right (darker green)
};

// Villager NPC cube vertices (blue)
static PosColorVertex villagerCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff0066FF}, // 0: Front-top-left (blue)
    { 1.0f,  1.0f,  1.0f, 0xff0066FF}, // 1: Front-top-right
    {-1.0f, -1.0f,  1.0f, 0xff0044CC}, // 2: Front-bottom-left (darker blue)
    { 1.0f, -1.0f,  1.0f, 0xff0044CC}, // 3: Front-bottom-right (darker blue)
    {-1.0f,  1.0f, -1.0f, 0xff0088FF}, // 4: Back-top-left (lighter blue)
    { 1.0f,  1.0f, -1.0f, 0xff0088FF}, // 5: Back-top-right (lighter blue)
    {-1.0f, -1.0f, -1.0f, 0xff0044CC}, // 6: Back-bottom-left (darker blue)
    { 1.0f, -1.0f, -1.0f, 0xff0044CC}  // 7: Back-bottom-right (darker blue)
};

// Merchant NPC cube vertices (orange)
static PosColorVertex merchantCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xffFFAA00}, // 0: Front-top-left (orange)
    { 1.0f,  1.0f,  1.0f, 0xffFFAA00}, // 1: Front-top-right
    {-1.0f, -1.0f,  1.0f, 0xffCC8800}, // 2: Front-bottom-left (darker orange)
    { 1.0f, -1.0f,  1.0f, 0xffCC8800}, // 3: Front-bottom-right (darker orange)
    {-1.0f,  1.0f, -1.0f, 0xffFFCC00}, // 4: Back-top-left (lighter orange)
    { 1.0f,  1.0f, -1.0f, 0xffFFCC00}, // 5: Back-top-right (lighter orange)
    {-1.0f, -1.0f, -1.0f, 0xffCC8800}, // 6: Back-bottom-left (darker orange)
    { 1.0f, -1.0f, -1.0f, 0xffCC8800}  // 7: Back-bottom-right (darker orange)
};

// Textured cube vertices
static PosTexVertex texCubeVertices[] = {
    // Front face
    {-1.0f,  1.0f,  1.0f, 0.0f, 0.0f}, // top-left
    { 1.0f,  1.0f,  1.0f, 1.0f, 0.0f}, // top-right
    {-1.0f, -1.0f,  1.0f, 0.0f, 1.0f}, // bottom-left
    { 1.0f, -1.0f,  1.0f, 1.0f, 1.0f}, // bottom-right
    
    // Back face
    {-1.0f,  1.0f, -1.0f, 0.0f, 0.0f}, // top-left
    { 1.0f,  1.0f, -1.0f, 1.0f, 0.0f}, // top-right
    {-1.0f, -1.0f, -1.0f, 0.0f, 1.0f}, // bottom-left
    { 1.0f, -1.0f, -1.0f, 1.0f, 1.0f}, // bottom-right
    
    // Top face
    {-1.0f,  1.0f, -1.0f, 0.0f, 0.0f}, // back-left
    { 1.0f,  1.0f, -1.0f, 1.0f, 0.0f}, // back-right
    {-1.0f,  1.0f,  1.0f, 0.0f, 1.0f}, // front-left
    { 1.0f,  1.0f,  1.0f, 1.0f, 1.0f}, // front-right
    
    // Bottom face
    {-1.0f, -1.0f, -1.0f, 0.0f, 0.0f}, // back-left
    { 1.0f, -1.0f, -1.0f, 1.0f, 0.0f}, // back-right
    {-1.0f, -1.0f,  1.0f, 0.0f, 1.0f}, // front-left
    { 1.0f, -1.0f,  1.0f, 1.0f, 1.0f}, // front-right
    
    // Left face
    {-1.0f,  1.0f, -1.0f, 0.0f, 0.0f}, // top-back
    {-1.0f,  1.0f,  1.0f, 1.0f, 0.0f}, // top-front
    {-1.0f, -1.0f, -1.0f, 0.0f, 1.0f}, // bottom-back
    {-1.0f, -1.0f,  1.0f, 1.0f, 1.0f}, // bottom-front
    
    // Right face
    { 1.0f,  1.0f,  1.0f, 0.0f, 0.0f}, // top-front
    { 1.0f,  1.0f, -1.0f, 1.0f, 0.0f}, // top-back
    { 1.0f, -1.0f,  1.0f, 0.0f, 1.0f}, // bottom-front
    { 1.0f, -1.0f, -1.0f, 1.0f, 1.0f}  // bottom-back
};

// Cube indices
static const uint16_t cubeIndices[] = {
    // Front face
    0, 1, 2, 1, 3, 2,
    // Back face
    4, 6, 5, 5, 6, 7,
    // Top face
    0, 4, 1, 1, 4, 5,
    // Bottom face
    2, 3, 6, 3, 7, 6,
    // Left face
    0, 2, 4, 2, 6, 4,
    // Right face
    1, 5, 3, 3, 5, 7
};

// Textured cube indices
static const uint16_t texCubeIndices[] = {
    // Front face
    0, 1, 2, 1, 3, 2,
    // Back face
    4, 5, 6, 5, 7, 6,
    // Top face
    8, 9, 10, 9, 11, 10,
    // Bottom face
    12, 13, 14, 13, 15, 14,
    // Left face
    16, 17, 18, 17, 19, 18,
    // Right face
    20, 21, 22, 21, 23, 22
};

// Biome system
enum class BiomeType {
    DESERT,      // Flat, sandy, minimal height variation
    MOUNTAINS,   // High elevation, steep terrain  
    SWAMP,       // Low, flat, some water areas
    GRASSLAND    // Rolling hills, moderate variation
};

// Forward declarations for biome texture functions
bgfx::TextureHandle create_biome_texture(BiomeType biome);

// Terrain system
struct TerrainVertex {
    float x, y, z;
    float u, v;
};

// Global ocean/water constants
static constexpr float SEA_LEVEL = 1.0f;  // Y coordinate for water surface
static constexpr float OCEAN_DEPTH_SCALE = 0.3f;  // How much to flatten terrain underwater

class TerrainChunk {
public:
    static constexpr int CHUNK_SIZE = 64;
    static constexpr float SCALE = 0.5f;
    static constexpr float HEIGHT_SCALE = 3.0f;
    
    BiomeType biome;
    int chunkX, chunkZ;  // Chunk coordinates in world space
    std::vector<TerrainVertex> vertices;
    std::vector<uint16_t> indices;
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
    bgfx::TextureHandle texture;  // Biome-specific texture
    
    // Water plane data
    bool hasWater;  // Does this chunk need water rendering?
    std::vector<TerrainVertex> waterVertices;
    std::vector<uint16_t> waterIndices;
    bgfx::VertexBufferHandle waterVbh;
    bgfx::IndexBufferHandle waterIbh;
    
    TerrainChunk(int cx, int cz, BiomeType biomeType) 
        : chunkX(cx), chunkZ(cz), biome(biomeType), hasWater(false) {
        vbh = BGFX_INVALID_HANDLE;
        ibh = BGFX_INVALID_HANDLE;
        texture = BGFX_INVALID_HANDLE;
        waterVbh = BGFX_INVALID_HANDLE;
        waterIbh = BGFX_INVALID_HANDLE;
    }
    
    ~TerrainChunk() {
        if (bgfx::isValid(vbh)) bgfx::destroy(vbh);
        if (bgfx::isValid(ibh)) bgfx::destroy(ibh);
        if (bgfx::isValid(texture)) bgfx::destroy(texture);
        if (bgfx::isValid(waterVbh)) bgfx::destroy(waterVbh);
        if (bgfx::isValid(waterIbh)) bgfx::destroy(waterIbh);
    }
    
    void generate() {
        vertices.clear();
        indices.clear();
        waterVertices.clear();
        waterIndices.clear();
        hasWater = false;
        
        // Generate vertices with biome-specific terrain
        generateBiomeTerrain();
        generateIndices();
        
        // Check if this chunk needs water and generate water plane if needed
        checkAndGenerateWater();
        
        // Validate chunk geometry
        validateChunkGeometry();
        
        std::cout << "Generated " << getBiomeName() << " chunk (" << chunkX << ", " << chunkZ 
                  << ") with " << vertices.size() << " vertices";
        if (hasWater) {
            std::cout << " (has water)";
        }
        std::cout << std::endl;
    }
    
private:
    // Global noise function that works across chunk boundaries
    float getGlobalNoise(float worldX, float worldZ) const {
        // Multiple octaves of noise for better variation
        float noise = 0.0f;
        
        // Large scale features (octave 1)
        noise += bx::sin(worldX * 0.01f) * bx::cos(worldZ * 0.012f) * 1.0f;
        
        // Medium scale features (octave 2)  
        noise += bx::sin(worldX * 0.03f + worldZ * 0.02f) * 0.5f;
        
        // Fine detail (octave 3)
        noise += bx::sin(worldX * 0.08f) * bx::cos(worldZ * 0.075f) * 0.25f;
        
        // Very fine detail (octave 4)
        noise += bx::sin(worldX * 0.15f + worldZ * 0.12f) * 0.125f;
        
        return noise * 0.4f; // Scale down total amplitude
    }
    
    void generateBiomeTerrain() {
        // Generate vertices using global world coordinates for seamless transitions
        
        // Generate vertices with biome-specific height patterns
        // Generate vertices including one extra row/column for seamless stitching
        for (int z = 0; z <= CHUNK_SIZE; z++) {
            for (int x = 0; x <= CHUNK_SIZE; x++) {
                // Calculate world position for this chunk vertex
                float worldX = (chunkX * CHUNK_SIZE + x) * SCALE;
                float worldZ = (chunkZ * CHUNK_SIZE + z) * SCALE;
                
                // Use global noise for seamless chunk transitions
                float globalNoise = getGlobalNoise(worldX, worldZ);
                float height = generateBiomeHeight(globalNoise, worldX, worldZ);
                
                TerrainVertex vertex;
                vertex.x = worldX;
                vertex.y = height;
                vertex.z = worldZ;
                vertex.u = float(x) / CHUNK_SIZE;
                vertex.v = float(z) / CHUNK_SIZE;
                
                vertices.push_back(vertex);
            }
        }
    }
    
    float generateBiomeHeight(float baseNoise, float worldX, float worldZ) {
        // Add secondary global noise layers for more detail (using world coordinates)
        float detailNoise1 = bx::sin(worldX * 0.05f) * bx::cos(worldZ * 0.04f);
        float detailNoise2 = bx::sin(worldX * 0.12f + worldZ * 0.1f);
        float fineNoise = bx::sin(worldX * 0.25f) * bx::cos(worldZ * 0.22f);
        
        // Calculate biome influence at this exact world position for smooth blending
        float biomeNoise = bx::sin(worldX * 0.001f) * bx::cos(worldZ * 0.0008f);
        biomeNoise += bx::sin(worldX * 0.0005f + worldZ * 0.0007f) * 0.3f;
        
        // Add ocean areas using large-scale noise
        float oceanNoise = bx::sin(worldX * 0.0003f) * bx::cos(worldZ * 0.0004f);
        oceanNoise += bx::sin(worldX * 0.0002f + worldZ * 0.0003f) * 0.5f;
        bool isOceanArea = oceanNoise < -0.6f;  // Specific areas are oceans
        
        // Calculate weights for each biome based on distance from thresholds
        float swampWeight = 1.0f - bx::clamp((biomeNoise + 0.3f) / 0.4f, 0.0f, 1.0f);  // Strong below -0.3
        float desertWeight = bx::max(0.0f, 1.0f - bx::abs(biomeNoise + 0.2f) / 0.2f);    // Peak at -0.2
        float grasslandWeight = bx::max(0.0f, 1.0f - bx::abs(biomeNoise - 0.05f) / 0.3f); // Peak at 0.05  
        float mountainWeight = bx::clamp((biomeNoise - 0.1f) / 0.3f, 0.0f, 1.0f);        // Strong above 0.2
        
        // Normalize weights so they sum to 1.0
        float totalWeight = swampWeight + desertWeight + grasslandWeight + mountainWeight;
        if (totalWeight > 0.0f) {
            swampWeight /= totalWeight;
            desertWeight /= totalWeight;
            grasslandWeight /= totalWeight;
            mountainWeight /= totalWeight;
        }
        
        // Calculate height for each biome type
        // Swamps: Low wetlands, oscillating around sea level for patches of water and land
        float swampVariation = bx::sin(worldX * 0.08f) * bx::cos(worldZ * 0.07f) * 0.8f; // Create patchy wetlands
        float swampHeight = baseNoise * HEIGHT_SCALE * 0.2f + swampVariation + detailNoise2 * HEIGHT_SCALE * 0.1f + 0.5f;
        
        // Deserts: Dry, above sea level with dunes
        float desertHeight = baseNoise * HEIGHT_SCALE * 1.2f + detailNoise1 * HEIGHT_SCALE * 0.3f + fineNoise * HEIGHT_SCALE * 0.1f + 3.0f;
        
        // Grasslands: Rolling hills, can have some low areas near water
        float grasslandHeight = baseNoise * HEIGHT_SCALE * 1.5f + detailNoise1 * HEIGHT_SCALE * 0.8f + detailNoise2 * HEIGHT_SCALE * 0.4f + fineNoise * HEIGHT_SCALE * 0.2f + 2.0f;
        
        // Mountains: High elevation, only deep valleys might have water
        float mountainHeight = (baseNoise + 0.2f) * HEIGHT_SCALE * 2.5f + detailNoise1 * HEIGHT_SCALE * 1.2f + detailNoise2 * HEIGHT_SCALE * 0.6f + fineNoise * HEIGHT_SCALE * 0.3f + 10.0f;
        
        // Blend heights based on biome weights for seamless transitions
        float blendedHeight = swampHeight * swampWeight + desertHeight * desertWeight + grasslandHeight * grasslandWeight + mountainHeight * mountainWeight;
        
        // If this is an ocean area, create underwater terrain
        if (isOceanArea) {
            // Ocean floor is below sea level
            float oceanDepth = 3.0f + baseNoise * 2.0f + detailNoise1 * 0.5f;
            blendedHeight = SEA_LEVEL - oceanDepth;
        }
        
        // Apply underwater flattening - terrain below sea level gets progressively flatter
        if (blendedHeight < SEA_LEVEL) {
            float depthBelowSeaLevel = SEA_LEVEL - blendedHeight;
            float flatteningFactor = 1.0f - bx::min(depthBelowSeaLevel * OCEAN_DEPTH_SCALE, 0.8f);
            blendedHeight = SEA_LEVEL - (depthBelowSeaLevel * flatteningFactor);
        }
        
        return blendedHeight;
    }
    
    void generateIndices() {
        // Generate indices for triangles with new vertex grid size
        const int vertexRowSize = CHUNK_SIZE + 1; // Account for extra row/column
        
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                int topLeft = z * vertexRowSize + x;
                int topRight = z * vertexRowSize + (x + 1);
                int bottomLeft = (z + 1) * vertexRowSize + x;
                int bottomRight = (z + 1) * vertexRowSize + (x + 1);
                
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
                
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }
    }
    
    void checkAndGenerateWater() {
        // Check if any terrain vertices are below sea level
        hasWater = false;
        for (const auto& vertex : vertices) {
            if (vertex.y < SEA_LEVEL) {
                hasWater = true;
                break;
            }
        }
        
        if (!hasWater) return;
        
        // Generate a simple water plane at sea level for this chunk
        waterVertices.clear();
        waterIndices.clear();
        
        // Create water plane vertices (simple grid at sea level)
        const int waterResolution = 8; // Lower resolution for water plane
        const float waterScale = float(CHUNK_SIZE) / float(waterResolution);
        
        for (int z = 0; z <= waterResolution; z++) {
            for (int x = 0; x <= waterResolution; x++) {
                TerrainVertex waterVertex;
                waterVertex.x = (chunkX * CHUNK_SIZE + x * waterScale) * SCALE;
                waterVertex.y = SEA_LEVEL - 5.0f; // Match the terrain offset
                waterVertex.z = (chunkZ * CHUNK_SIZE + z * waterScale) * SCALE;
                waterVertex.u = float(x) / waterResolution;
                waterVertex.v = float(z) / waterResolution;
                
                waterVertices.push_back(waterVertex);
            }
        }
        
        // Generate water indices
        const int waterVertexRowSize = waterResolution + 1;
        for (int z = 0; z < waterResolution; z++) {
            for (int x = 0; x < waterResolution; x++) {
                int topLeft = z * waterVertexRowSize + x;
                int topRight = z * waterVertexRowSize + (x + 1);
                int bottomLeft = (z + 1) * waterVertexRowSize + x;
                int bottomRight = (z + 1) * waterVertexRowSize + (x + 1);
                
                waterIndices.push_back(topLeft);
                waterIndices.push_back(bottomLeft);
                waterIndices.push_back(topRight);
                
                waterIndices.push_back(topRight);
                waterIndices.push_back(bottomLeft);
                waterIndices.push_back(bottomRight);
            }
        }
    }
    
    void validateChunkGeometry() {
        bool hasIssues = false;
        float minY = 1000000.0f, maxY = -1000000.0f;
        int invalidVertices = 0;
        
        // Check for invalid vertices
        for (size_t i = 0; i < vertices.size(); i++) {
            const auto& v = vertices[i];
            
            // Check for NaN or infinite values
            if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
                std::cerr << "ERROR: Chunk (" << chunkX << ", " << chunkZ << ") vertex " << i 
                          << " has invalid position: (" << v.x << ", " << v.y << ", " << v.z << ")" << std::endl;
                invalidVertices++;
                hasIssues = true;
            }
            
            // Track height bounds
            if (std::isfinite(v.y)) {
                minY = bx::min(minY, v.y);
                maxY = bx::max(maxY, v.y);
            }
            
            // Check for extreme positions
            if (bx::abs(v.x) > 10000.0f || bx::abs(v.z) > 10000.0f) {
                std::cerr << "WARNING: Chunk (" << chunkX << ", " << chunkZ << ") vertex " << i 
                          << " at extreme position: (" << v.x << ", " << v.y << ", " << v.z << ")" << std::endl;
                hasIssues = true;
            }
        }
        
        // Check height range
        float heightRange = maxY - minY;
        if (heightRange > 100.0f) {
            std::cerr << "WARNING: Chunk (" << chunkX << ", " << chunkZ << ") has extreme height range: " 
                      << minY << " to " << maxY << " (range: " << heightRange << ")" << std::endl;
            hasIssues = true;
        }
        
        // Check indices
        for (size_t i = 0; i < indices.size(); i++) {
            if (indices[i] >= vertices.size()) {
                std::cerr << "ERROR: Chunk (" << chunkX << ", " << chunkZ << ") index " << i 
                          << " value " << indices[i] << " exceeds vertex count " << vertices.size() << std::endl;
                hasIssues = true;
            }
        }
        
        if (hasIssues) {
            std::cerr << "CHUNK VALIDATION FAILED for (" << chunkX << ", " << chunkZ << ")" << std::endl;
        }
    }

public:
    const char* getBiomeName() const {
        switch (biome) {
            case BiomeType::DESERT: return "Desert";
            case BiomeType::MOUNTAINS: return "Mountains";
            case BiomeType::SWAMP: return "Swamp";
            case BiomeType::GRASSLAND: return "Grassland";
            default: return "Unknown";
        }
    }
    
    void createBuffers() {
        if (vertices.empty() || indices.empty()) {
            std::cerr << "ERROR: Chunk (" << chunkX << ", " << chunkZ << ") has empty vertices or indices!" << std::endl;
            std::cerr << "Vertices: " << vertices.size() << ", Indices: " << indices.size() << std::endl;
            return;
        }
        
        const bgfx::Memory* vertexMem = bgfx::copy(vertices.data(), vertices.size() * sizeof(TerrainVertex));
        const bgfx::Memory* indexMem = bgfx::copy(indices.data(), indices.size() * sizeof(uint16_t));
        
        bgfx::VertexLayout terrainLayout;
        terrainLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        
        vbh = bgfx::createVertexBuffer(vertexMem, terrainLayout);
        ibh = bgfx::createIndexBuffer(indexMem);
        
        // Check if buffer creation succeeded
        if (!bgfx::isValid(vbh)) {
            std::cerr << "ERROR: Failed to create vertex buffer for chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
        }
        if (!bgfx::isValid(ibh)) {
            std::cerr << "ERROR: Failed to create index buffer for chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
        }
        
        // Create biome-specific texture
        texture = create_biome_texture(biome);
        if (!bgfx::isValid(texture)) {
            std::cerr << "ERROR: Failed to create texture for chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
        } else {
            std::cout << "Successfully created " << getBiomeName() << " chunk (" << chunkX << ", " << chunkZ 
                      << ") with " << vertices.size() << " vertices and " << indices.size() << " indices" << std::endl;
        }
        
        // Create water buffers if needed
        if (hasWater && !waterVertices.empty() && !waterIndices.empty()) {
            const bgfx::Memory* waterVertexMem = bgfx::copy(waterVertices.data(), waterVertices.size() * sizeof(TerrainVertex));
            const bgfx::Memory* waterIndexMem = bgfx::copy(waterIndices.data(), waterIndices.size() * sizeof(uint16_t));
            
            waterVbh = bgfx::createVertexBuffer(waterVertexMem, terrainLayout);
            waterIbh = bgfx::createIndexBuffer(waterIndexMem);
            
            if (!bgfx::isValid(waterVbh) || !bgfx::isValid(waterIbh)) {
                std::cerr << "ERROR: Failed to create water buffers for chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
            }
        }
    }
    
    float getHeightAt(float worldX, float worldZ) const {
        // Convert world coordinates to local chunk coordinates
        float localX = worldX / SCALE - (chunkX * CHUNK_SIZE);
        float localZ = worldZ / SCALE - (chunkZ * CHUNK_SIZE);
        
        // Clamp to chunk bounds  
        if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE) {
            return 0.0f;
        }
        
        int x1 = (int)localX;
        int z1 = (int)localZ;
        int x2 = bx::min(x1 + 1, CHUNK_SIZE);
        int z2 = bx::min(z1 + 1, CHUNK_SIZE);
        
        float fx = localX - x1;
        float fz = localZ - z1;
        
        // Get heights at four corners (with new vertex row size)
        const int vertexRowSize = CHUNK_SIZE + 1;
        float h1 = vertices[z1 * vertexRowSize + x1].y;
        float h2 = vertices[z1 * vertexRowSize + x2].y;
        float h3 = vertices[z2 * vertexRowSize + x1].y;
        float h4 = vertices[z2 * vertexRowSize + x2].y;
        
        // Bilinear interpolation
        float top = h1 * (1 - fx) + h2 * fx;
        float bottom = h3 * (1 - fx) + h4 * fx;
        return top * (1 - fz) + bottom * fz;
    }
};

// Forward declaration
void render_object_at_position(bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, 
                              bgfx::ProgramHandle program, bgfx::TextureHandle texture, 
                              bgfx::UniformHandle texUniform, const float* modelMatrix);

// Resource node system

// Forward declarations

// Chunk management system
class ChunkManager {
public:
    static constexpr int RENDER_DISTANCE = 2; // Chunks in each direction (5x5 grid)
    
private:
    std::unordered_map<uint64_t, std::unique_ptr<TerrainChunk>> loadedChunks;
    std::vector<ResourceNode>* worldResourceNodes; // Pointer to global resource nodes
    std::vector<NPC>* worldNPCs; // Pointer to global NPCs
    int playerChunkX = 0;
    int playerChunkZ = 0;
    
    // Helper function to convert chunk coordinates to unique key
    uint64_t getChunkKey(int chunkX, int chunkZ) const {
        // Handle negative coordinates properly by treating as signed
        uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(chunkX));
        uint64_t z = static_cast<uint64_t>(static_cast<uint32_t>(chunkZ));
        return (x << 32) | z;
    }
    
    // Determine biome for a chunk based on its world coordinates
    BiomeType getBiomeForChunk(int chunkX, int chunkZ) const {
        return getBiomeAtWorldPos(chunkX * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE, 
                                  chunkZ * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE);
    }
    
    // Get biome at any world position (used for blending)
    BiomeType getBiomeAtWorldPos(float worldX, float worldZ) const {
        // Use very large scale for biome assignment to create big continuous regions
        float biomeNoise = bx::sin(worldX * 0.001f) * bx::cos(worldZ * 0.0008f);
        biomeNoise += bx::sin(worldX * 0.0005f + worldZ * 0.0007f) * 0.3f;
        
        // Smoother thresholds for gradual transitions
        if (biomeNoise < -0.3f) return BiomeType::SWAMP;
        if (biomeNoise < -0.1f) return BiomeType::DESERT; 
        if (biomeNoise < 0.2f) return BiomeType::GRASSLAND;
        return BiomeType::MOUNTAINS;
    }
    
    // Generate procedural resource nodes for a chunk
    void generateResourcesForChunk(int chunkX, int chunkZ) {
        if (!worldResourceNodes) return;
        
        BiomeType chunkBiome = getBiomeForChunk(chunkX, chunkZ);
        
        // World coordinates for this chunk
        float chunkWorldX = chunkX * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
        float chunkWorldZ = chunkZ * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
        
        // Generate resource nodes based on biome
        int nodeCount = 0;
        float density = 0.3f; // Base density
        
        // Biome-specific resource generation
        switch (chunkBiome) {
            case BiomeType::MOUNTAINS:
                // Mountains have iron and stone
                density = 0.5f; // Higher density in mountains
                for (int attempts = 0; attempts < 8; attempts++) {
                    float seed = (chunkX * 73.0f + chunkZ * 47.0f + attempts * 23.0f);
                    float noiseValue = bx::sin(seed * 0.1f) * bx::cos(seed * 0.13f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.7f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.8f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        ResourceType resourceType = (bx::sin(seed * 0.9f) > 0.0f) ? ResourceType::IRON : ResourceType::STONE;
                        float worldY = getHeightAt(worldX, worldZ) + 0.5f - 5.0f;
                        
                        worldResourceNodes->emplace_back(worldX, worldY, worldZ, resourceType, 100);
                        nodeCount++;
                    }
                }
                break;
                
            case BiomeType::DESERT:
                // Desert has copper and stone
                density = 0.25f;
                for (int attempts = 0; attempts < 6; attempts++) {
                    float seed = (chunkX * 67.0f + chunkZ * 53.0f + attempts * 29.0f);
                    float noiseValue = bx::sin(seed * 0.15f) * bx::cos(seed * 0.11f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.6f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.7f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        ResourceType resourceType = (bx::sin(seed * 0.8f) > 0.2f) ? ResourceType::COPPER : ResourceType::STONE;
                        float worldY = getHeightAt(worldX, worldZ) + 0.5f - 5.0f;
                        
                        worldResourceNodes->emplace_back(worldX, worldY, worldZ, resourceType, 100);
                        nodeCount++;
                    }
                }
                break;
                
            case BiomeType::GRASSLAND:
                // Grassland has balanced resources
                density = 0.35f;
                for (int attempts = 0; attempts < 7; attempts++) {
                    float seed = (chunkX * 71.0f + chunkZ * 41.0f + attempts * 31.0f);
                    float noiseValue = bx::sin(seed * 0.12f) * bx::cos(seed * 0.14f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.65f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.75f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        // Balanced distribution of all resource types
                        float typeNoise = bx::sin(seed * 1.1f);
                        ResourceType resourceType;
                        if (typeNoise < -0.3f) resourceType = ResourceType::COPPER;
                        else if (typeNoise < 0.3f) resourceType = ResourceType::IRON;
                        else resourceType = ResourceType::STONE;
                        
                        float worldY = getHeightAt(worldX, worldZ) + 0.5f - 5.0f;
                        
                        worldResourceNodes->emplace_back(worldX, worldY, worldZ, resourceType, 100);
                        nodeCount++;
                    }
                }
                break;
                
            case BiomeType::SWAMP:
                // Swamp has mainly iron and occasional stone
                density = 0.2f; // Lower density in swamps
                for (int attempts = 0; attempts < 5; attempts++) {
                    float seed = (chunkX * 61.0f + chunkZ * 59.0f + attempts * 37.0f);
                    float noiseValue = bx::sin(seed * 0.18f) * bx::cos(seed * 0.09f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.55f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.85f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        ResourceType resourceType = (bx::sin(seed * 1.2f) > 0.4f) ? ResourceType::IRON : ResourceType::STONE;
                        float worldY = getHeightAt(worldX, worldZ) + 0.5f - 5.0f;
                        
                        worldResourceNodes->emplace_back(worldX, worldY, worldZ, resourceType, 100);
                        nodeCount++;
                    }
                }
                break;
        }
        
        if (nodeCount > 0) {
            std::cout << "Generated " << nodeCount << " resource nodes in " 
                      << getBiomeName(chunkBiome) << " chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
        }
    }
    
    // Generate procedural NPCs for a chunk
    void generateNPCsForChunk(int chunkX, int chunkZ) {
        if (!worldNPCs) return;
        
        BiomeType chunkBiome = getBiomeForChunk(chunkX, chunkZ);
        
        // World coordinates for this chunk
        float chunkWorldX = chunkX * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
        float chunkWorldZ = chunkZ * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
        
        // Generate NPCs based on biome (lower density than resources)
        int npcCount = 0;
        float density = 0.1f; // Much lower base density than resources
        
        // Biome-specific NPC generation
        switch (chunkBiome) {
            case BiomeType::GRASSLAND:
                // Grasslands have the most NPCs (villages)
                density = 0.15f;
                for (int attempts = 0; attempts < 3; attempts++) {
                    float seed = (chunkX * 89.0f + chunkZ * 67.0f + attempts * 43.0f);
                    float noiseValue = bx::sin(seed * 0.08f) * bx::cos(seed * 0.12f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.5f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.6f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        NPCType npcType;
                        float typeNoise = bx::sin(seed * 1.3f);
                        if (typeNoise < -0.2f) npcType = NPCType::MERCHANT;
                        else if (typeNoise < 0.4f) npcType = NPCType::VILLAGER;
                        else npcType = NPCType::WANDERER;
                        
                        float worldY = getHeightAt(worldX, worldZ) + 0.8f - 5.0f;
                        
                        worldNPCs->emplace_back(worldX, worldY, worldZ, npcType);
                        npcCount++;
                    }
                }
                break;
                
            case BiomeType::DESERT:
                // Desert has wanderers and occasional merchants
                density = 0.08f;
                for (int attempts = 0; attempts < 2; attempts++) {
                    float seed = (chunkX * 97.0f + chunkZ * 73.0f + attempts * 47.0f);
                    float noiseValue = bx::sin(seed * 0.1f) * bx::cos(seed * 0.09f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.4f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.7f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        NPCType npcType = (bx::sin(seed * 0.9f) > 0.3f) ? NPCType::WANDERER : NPCType::MERCHANT;
                        float worldY = getHeightAt(worldX, worldZ) + 0.8f - 5.0f;
                        
                        worldNPCs->emplace_back(worldX, worldY, worldZ, npcType);
                        npcCount++;
                    }
                }
                break;
                
            case BiomeType::MOUNTAINS:
                // Mountains have few NPCs, mostly wanderers
                density = 0.05f;
                for (int attempts = 0; attempts < 2; attempts++) {
                    float seed = (chunkX * 83.0f + chunkZ * 79.0f + attempts * 53.0f);
                    float noiseValue = bx::sin(seed * 0.15f) * bx::cos(seed * 0.07f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.6f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.8f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        NPCType npcType = NPCType::WANDERER; // Only wanderers in mountains
                        float worldY = getHeightAt(worldX, worldZ) + 0.8f - 5.0f;
                        
                        worldNPCs->emplace_back(worldX, worldY, worldZ, npcType);
                        npcCount++;
                    }
                }
                break;
                
            case BiomeType::SWAMP:
                // Swamps have very few NPCs
                density = 0.03f;
                for (int attempts = 0; attempts < 1; attempts++) {
                    float seed = (chunkX * 101.0f + chunkZ * 103.0f + attempts * 59.0f);
                    float noiseValue = bx::sin(seed * 0.18f) * bx::cos(seed * 0.06f);
                    
                    if (noiseValue > (1.0f - density)) {
                        float offsetX = (bx::sin(seed * 0.3f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float offsetZ = (bx::cos(seed * 0.9f) * 0.5f + 0.5f) * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                        float worldX = chunkWorldX + offsetX;
                        float worldZ = chunkWorldZ + offsetZ;
                        
                        NPCType npcType = NPCType::WANDERER; // Only wanderers in swamps
                        float worldY = getHeightAt(worldX, worldZ) + 0.8f - 5.0f;
                        
                        worldNPCs->emplace_back(worldX, worldY, worldZ, npcType);
                        npcCount++;
                    }
                }
                break;
        }
        
        if (npcCount > 0) {
            std::cout << "Generated " << npcCount << " NPCs in " 
                      << getBiomeName(chunkBiome) << " chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
        }
    }
    
    // Helper function to get biome name
    const char* getBiomeName(BiomeType biome) const {
        switch (biome) {
            case BiomeType::DESERT: return "Desert";
            case BiomeType::MOUNTAINS: return "Mountains";
            case BiomeType::SWAMP: return "Swamp";
            case BiomeType::GRASSLAND: return "Grassland";
            default: return "Unknown";
        }
    }
    
public:
    ChunkManager() = default;
    
    // Set pointer to global resource nodes vector
    void setResourceNodesPointer(std::vector<ResourceNode>* nodes) {
        worldResourceNodes = nodes;
    }
    
    // Set pointer to global NPCs vector
    void setNPCsPointer(std::vector<NPC>* npcs) {
        worldNPCs = npcs;
    }
    
    // Force initial chunk loading around player position
    void forceInitialChunkLoad(float playerX, float playerZ) {
        playerChunkX = (int)bx::floor(playerX / (TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE));
        playerChunkZ = (int)bx::floor(playerZ / (TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE));
        
        std::cout << "Force loading initial chunks around player at chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;
        
        // Load chunks in render distance
        loadChunksAroundPlayer();
    }
    
    // Update chunks around player position
    void updateChunksAroundPlayer(float playerX, float playerZ) {
        // Calculate which chunk the player is in
        int newPlayerChunkX = (int)bx::floor(playerX / (TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE));
        int newPlayerChunkZ = (int)bx::floor(playerZ / (TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE));
        
        // Only update if player moved to a new chunk
        if (newPlayerChunkX != playerChunkX || newPlayerChunkZ != playerChunkZ) {
            playerChunkX = newPlayerChunkX;
            playerChunkZ = newPlayerChunkZ;
            
            std::cout << "Player entered chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;
            
            // Load chunks in render distance
            loadChunksAroundPlayer();
            
            // Unload distant chunks
            unloadDistantChunks();
        }
    }
    
    // Get height at world coordinates from appropriate chunk
    float getHeightAt(float worldX, float worldZ) const {
        int chunkX = (int)bx::floor(worldX / (TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE));
        int chunkZ = (int)bx::floor(worldZ / (TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE));
        
        uint64_t key = getChunkKey(chunkX, chunkZ);
        auto it = loadedChunks.find(key);
        
        if (it != loadedChunks.end()) {
            return it->second->getHeightAt(worldX, worldZ);
        }
        
        return 0.0f; // Default height if chunk not loaded
    }
    
    // Render all loaded chunks
    void renderChunks(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform) {
        int renderedChunks = 0;
        int skippedChunks = 0;
        static int debugFrameCount = 0;
        bool shouldDebug = (debugFrameCount % 300 == 0); // Debug every 5 seconds at 60fps
        
        if (shouldDebug) {
            std::cout << "\n=== CHUNK RENDER DEBUG ===" << std::endl;
            std::cout << "Total loaded chunks: " << loadedChunks.size() << std::endl;
        }
        
        for (const auto& pair : loadedChunks) {
            const auto& chunk = pair.second;
            
            // Debug chunk info
            if (shouldDebug) {
                float worldX = chunk->chunkX * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                float worldZ = chunk->chunkZ * TerrainChunk::CHUNK_SIZE * TerrainChunk::SCALE;
                std::cout << "Chunk (" << chunk->chunkX << ", " << chunk->chunkZ << ") -> World pos (" 
                          << worldX << ", " << worldZ << ") - " << chunk->getBiomeName();
            }
            
            // Skip chunks with invalid buffers
            if (!bgfx::isValid(chunk->vbh) || !bgfx::isValid(chunk->ibh) || !bgfx::isValid(chunk->texture)) {
                if (shouldDebug) {
                    std::cout << " - SKIPPED (invalid buffers: vbh=" << bgfx::isValid(chunk->vbh) 
                              << ", ibh=" << bgfx::isValid(chunk->ibh) 
                              << ", tex=" << bgfx::isValid(chunk->texture) << ")" << std::endl;
                }
                skippedChunks++;
                continue;
            }
            
            if (shouldDebug) {
                std::cout << " - RENDERED" << std::endl;
            }
            
            // Create transform matrix for chunk
            float chunkMatrix[16], translation[16];
            bx::mtxTranslate(translation, 0.0f, -5.0f, 0.0f); // Same terrain offset as before
            bx::mtxIdentity(chunkMatrix);
            bx::mtxMul(chunkMatrix, chunkMatrix, translation);
            
            // Set terrain rendering state
            uint64_t terrainState = BGFX_STATE_DEFAULT;
            terrainState &= ~BGFX_STATE_CULL_MASK;
            bgfx::setState(terrainState);
            
            // Render this chunk with its biome-specific texture
            render_object_at_position(chunk->vbh, chunk->ibh, program, chunk->texture, texUniform, chunkMatrix);
            renderedChunks++;
        }
        
        if (shouldDebug) {
            std::cout << "Rendered: " << renderedChunks << ", Skipped: " << skippedChunks << std::endl;
            std::cout << "=========================" << std::endl;
        }
        
        debugFrameCount++;
    }
    
    // Render water for all loaded chunks that have water
    void renderWater(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform, bgfx::TextureHandle waterTexture) {
        for (const auto& pair : loadedChunks) {
            const auto& chunk = pair.second;
            
            // Skip chunks without water
            if (!chunk->hasWater || !bgfx::isValid(chunk->waterVbh) || !bgfx::isValid(chunk->waterIbh)) {
                continue;
            }
            
            // Create transform matrix for water (no offset needed since we included it in vertex generation)
            float waterMatrix[16];
            bx::mtxIdentity(waterMatrix);
            
            // Set water state before rendering
            uint64_t waterState = BGFX_STATE_DEFAULT;
            waterState |= BGFX_STATE_BLEND_ALPHA;
            waterState &= ~BGFX_STATE_CULL_MASK;
            bgfx::setState(waterState);
            
            // Render water plane with transparency
            render_object_at_position(chunk->waterVbh, chunk->waterIbh, program, waterTexture, texUniform, waterMatrix);
        }
    }
    
    // Get chunk info for debugging
    std::vector<std::string> getLoadedChunkInfo() const {
        std::vector<std::string> info;
        for (const auto& pair : loadedChunks) {
            const auto& chunk = pair.second;
            info.push_back(std::to_string(chunk->chunkX) + "," + std::to_string(chunk->chunkZ) + 
                          " (" + chunk->getBiomeName() + ")");
        }
        return info;
    }
    
private:
    void loadChunksAroundPlayer() {
        std::cout << "Loading chunks in " << (2*RENDER_DISTANCE+1) << "x" << (2*RENDER_DISTANCE+1) 
                  << " grid around player chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;
        
        for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
            for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                uint64_t key = getChunkKey(x, z);
                
                // Only create chunk if it doesn't exist
                if (loadedChunks.find(key) == loadedChunks.end()) {
                    std::cout << "  Loading chunk (" << x << ", " << z << ")" << std::endl;
                    BiomeType biome = getBiomeForChunk(x, z);
                    auto chunk = std::make_unique<TerrainChunk>(x, z, biome);
                    chunk->generate();
                    chunk->createBuffers();
                    
                    loadedChunks[key] = std::move(chunk);
                    
                    // Generate resource nodes for this new chunk
                    generateResourcesForChunk(x, z);
                    
                    // Generate NPCs for this new chunk
                    generateNPCsForChunk(x, z);
                } else {
                    std::cout << "  Chunk (" << x << ", " << z << ") already exists" << std::endl;
                }
            }
        }
        std::cout << "Finished loading chunks. Total loaded: " << loadedChunks.size() << std::endl;
    }
    
    void unloadDistantChunks() {
        auto it = loadedChunks.begin();
        while (it != loadedChunks.end()) {
            const auto& chunk = it->second;
            int deltaX = bx::abs(chunk->chunkX - playerChunkX);
            int deltaZ = bx::abs(chunk->chunkZ - playerChunkZ);
            
            // Unload if outside render distance + buffer
            if (deltaX > RENDER_DISTANCE + 1 || deltaZ > RENDER_DISTANCE + 1) {
                std::cout << "Unloading chunk (" << chunk->chunkX << ", " << chunk->chunkZ << ")" << std::endl;
                it = loadedChunks.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// Skill types

// Player system

// Player methods that need ChunkManager (implemented here due to dependencies)
void Player::setTarget(float x, float z, const ChunkManager& chunkManager, bool sprint) {
    targetPosition.x = x;
    targetPosition.z = z;
    targetPosition.y = chunkManager.getHeightAt(x, z) + size - 5.0f; // Account for terrain offset
    hasTarget = true;
    isSprinting = sprint;
}

void Player::update(const ChunkManager& chunkManager, float currentTime, float deltaTime) {
    // Update hit flash timer
    if (hitFlashTimer > 0) {
        hitFlashTimer -= deltaTime;
        if (hitFlashTimer < 0) hitFlashTimer = 0;
    }
    
    // Track movement for Athletics XP
    bx::Vec3 oldPosition = position;
    
    // Handle combat if we have a target
    if (combatTarget && combatTarget->isActive) {
        float dx = combatTarget->position.x - position.x;
        float dz = combatTarget->position.z - position.z;
        float distance = bx::sqrt(dx * dx + dz * dz);
        
        // Check if we're in combat range
        if (distance <= 3.0f) {
            inCombat = true;
            hasTarget = false; // Stop normal movement
            
            // Maintain combat distance
            if (distance > 2.5f) {
                // Move closer
                position.x += (dx / distance) * moveSpeed * 2.0f;
                position.z += (dz / distance) * moveSpeed * 2.0f;
            } else if (distance < 1.5f) {
                // Back up
                position.x -= (dx / distance) * moveSpeed;
                position.z -= (dz / distance) * moveSpeed;
            }
            
            // Attack if cooldown is ready
            if (currentTime - lastAttackTime >= attackCooldown) {
                // RNG for hit/miss
                float hitRoll = (rand() % 100) / 100.0f;
                float dodgeRoll = (rand() % 100) / 100.0f;
                
                if (hitRoll < hitChance && dodgeRoll > combatTarget->dodgeChance) {
                    // Hit! Apply Unarmed skill modifier to damage
                    float unarmedModifier = skills.getSkill(SkillType::UNARMED).getModifier();
                    int actualDamage = (int)(attackDamage * unarmedModifier);
                    combatTarget->takeDamage(actualDamage, currentTime);
                    std::cout << "Player hits " << combatTarget->getTypeName() << " for " << actualDamage << " damage!" << std::endl;
                    
                    // Award Unarmed XP for successful hits
                    skills.getSkill(SkillType::UNARMED).addExperience(5.0f);
                } else {
                    std::cout << "Player misses!" << std::endl;
                    // Small XP even for misses to encourage practice
                    skills.getSkill(SkillType::UNARMED).addExperience(1.0f);
                }
                
                lastAttackTime = currentTime;
            }
        } else if (distance > 15.0f || !combatTarget->isActive) {
            // Too far or target dead, exit combat
            combatTarget = nullptr;
            inCombat = false;
        } else {
            // Not in combat range yet - keep moving toward target
            // Update target position in case NPC moved
            targetPosition = combatTarget->position;
            if (!hasTarget) {
                hasTarget = true;
                isSprinting = true; // Sprint to combat
            }
        }
    } else {
        inCombat = false;
    }
    
    // Normal movement when not in active combat
    if (hasTarget && !inCombat) {
        bx::Vec3 direction = {
            targetPosition.x - position.x,
            0.0f,
            targetPosition.z - position.z
        };
        
        float distance = bx::sqrt(direction.x * direction.x + direction.z * direction.z);
        
        if (distance < 0.1f) {
            position = targetPosition;
            hasTarget = false;
            isSprinting = false;
        } else {
            direction.x /= distance;
            direction.z /= distance;
            
            // Apply Athletics modifier to speed
            float athleticsModifier = skills.getSkill(SkillType::ATHLETICS).getModifier();
            float currentSpeed = (isSprinting ? sprintSpeed : moveSpeed) * athleticsModifier;
            position.x += direction.x * currentSpeed;
            position.z += direction.z * currentSpeed;
            position.y = chunkManager.getHeightAt(position.x, position.z) + size - 5.0f; // Account for terrain offset
        }
    }
    
    // Always update Y position
    position.y = chunkManager.getHeightAt(position.x, position.z) + size - 5.0f;
    
    // Calculate distance traveled and award Athletics XP
    float dx = position.x - oldPosition.x;
    float dz = position.z - oldPosition.z;
    float distance = bx::sqrt(dx * dx + dz * dz);
    
    if (distance > 0.001f) { // Only if we actually moved
        distanceTraveled += distance;
        
        // Award XP every unit of distance traveled
        float xpPerUnit = isSprinting ? 2.0f : 0.5f; // More XP for running
        if (distanceTraveled >= 1.0f) {
            skills.getSkill(SkillType::ATHLETICS).addExperience(xpPerUnit * distanceTraveled);
            distanceTraveled = 0.0f;
        }
    }
}

// NPC update method implementation (needs Player to be fully defined)

// Player inventory system

// Ray casting for mouse picking
struct Ray {
    bx::Vec3 origin;
    bx::Vec3 direction;
};

Ray createRayFromMouse(float mouseX, float mouseY, int screenWidth, int screenHeight, 
                       const float* viewMatrix, const float* projMatrix) {
    // Convert mouse coordinates to NDC using BGFX method
    float mouseXNDC = (mouseX / (float)screenWidth) * 2.0f - 1.0f;
    float mouseYNDC = ((screenHeight - mouseY) / (float)screenHeight) * 2.0f - 1.0f;
    
    // Create view-projection matrix and its inverse
    float viewProj[16];
    bx::mtxMul(viewProj, viewMatrix, projMatrix);
    float invViewProj[16];
    bx::mtxInverse(invViewProj, viewProj);
    
    // Unproject NDC coordinates to world space using BGFX method
    const bx::Vec3 pickEye = bx::mulH({mouseXNDC, mouseYNDC, 0.0f}, invViewProj);
    const bx::Vec3 pickAt = bx::mulH({mouseXNDC, mouseYNDC, 1.0f}, invViewProj);
    
    // Calculate ray direction
    bx::Vec3 rayDirection = {
        pickAt.x - pickEye.x,
        pickAt.y - pickEye.y,
        pickAt.z - pickEye.z
    };
    rayDirection = bx::normalize(rayDirection);
    
    // Debug output disabled for hover system
    // std::cout << "Mouse: (" << mouseX << ", " << mouseY << ") -> NDC: (" << mouseXNDC << ", " << mouseYNDC << ")" << std::endl;
    // std::cout << "Ray origin: (" << pickEye.x << ", " << pickEye.y << ", " << pickEye.z << ")" << std::endl;
    // std::cout << "Ray direction: (" << rayDirection.x << ", " << rayDirection.y << ", " << rayDirection.z << ")" << std::endl;
    
    Ray ray = {
        {pickEye.x, pickEye.y, pickEye.z}, 
        {rayDirection.x, rayDirection.y, rayDirection.z}
    };
    
    return ray;
}

bool rayTerrainIntersection(const Ray& ray, const ChunkManager& chunkManager, bx::Vec3& hitPoint) {
    float t = 0.0f;
    float maxDistance = 200.0f;
    float stepSize = 0.1f;
    
    // Account for terrain offset in rendering (-5.0f in Y)
    const float TERRAIN_Y_OFFSET = -5.0f;
    
    // Debug output disabled for hover system
    // std::cout << "  Ray intersection - Origin: (" << ray.origin.x << ", " << ray.origin.y << ", " << ray.origin.z << ")" << std::endl;
    // std::cout << "  Ray direction: (" << ray.direction.x << ", " << ray.direction.y << ", " << ray.direction.z << ")" << std::endl;
    
    int intersectionChecks = 0;
    
    while (t < maxDistance) {
        bx::Vec3 currentPoint = {
            ray.origin.x + ray.direction.x * t,
            ray.origin.y + ray.direction.y * t,
            ray.origin.z + ray.direction.z * t
        };
        
        float rawTerrainHeight = chunkManager.getHeightAt(currentPoint.x, currentPoint.z);
        float actualTerrainHeight = rawTerrainHeight + TERRAIN_Y_OFFSET;
        
        // Check if we're close to or below terrain (accounting for offset)
        if (currentPoint.y <= actualTerrainHeight + 0.1f) {
            hitPoint.x = currentPoint.x;
            hitPoint.z = currentPoint.z;
            hitPoint.y = actualTerrainHeight;
            
            // std::cout << "  Intersection found at t=" << t << ", point=(" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")" << std::endl;
            return true;
        }
        
        intersectionChecks++;
        t += stepSize;
        
        // Debug first few checks - disabled for hover system
        // if (intersectionChecks <= 3) {
        //     std::cout << "  Check " << intersectionChecks << ": t=" << t << ", point=(" << currentPoint.x << ", " << currentPoint.y << ", " << currentPoint.z << "), raw_height=" << rawTerrainHeight << ", actual_height=" << actualTerrainHeight << std::endl;
        // }
        
        // Early exit if we've gone too far below terrain
        if (intersectionChecks > 50 && currentPoint.y < actualTerrainHeight - 10.0f) {
            break;
        }
    }
    
    // std::cout << "  No intersection found after " << intersectionChecks << " checks" << std::endl;
    return false;
}

// Shader vertex layouts
static bgfx::VertexLayout layout;
static bgfx::VertexLayout texLayout;

// Get BGFX renderer type based on platform
bgfx::RendererType::Enum get_renderer_type() {
#if BX_PLATFORM_OSX
    return bgfx::RendererType::Metal;
#elif BX_PLATFORM_WINDOWS
    return bgfx::RendererType::Direct3D11;
#else
    return bgfx::RendererType::OpenGL;
#endif
}

// Load a PNG texture using stb_image
bgfx::TextureHandle load_png_texture(const char* filePath) {
    uint64_t textureFlags = BGFX_TEXTURE_NONE;
    textureFlags |= BGFX_SAMPLER_MIN_ANISOTROPIC;
    textureFlags |= BGFX_SAMPLER_MAG_ANISOTROPIC;
    
    int width, height, channels;
    
    stbi_set_flip_vertically_on_load(true);
    unsigned char* imageData = stbi_load(filePath, &width, &height, &channels, STBI_rgb_alpha);
    
    if (!imageData) {
        std::cerr << "Failed to load texture: " << filePath << std::endl;
        std::cerr << "stb_image error: " << stbi_failure_reason() << std::endl;
        return BGFX_INVALID_HANDLE;
    }
    
    std::cout << "Loaded texture: " << filePath << " (" << width << "x" << height << ", " 
              << channels << " channels)" << std::endl;
    
    uint32_t size = width * height * 4;
    const bgfx::Memory* mem = bgfx::copy(imageData, size);
    stbi_image_free(imageData);
    
    bgfx::TextureHandle handle = bgfx::createTexture2D(
        width, height, 
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        textureFlags,
        mem
    );
    
    if (!bgfx::isValid(handle)) {
        std::cerr << "Failed to create BGFX texture from PNG data" << std::endl;
    }
    
    return handle;
}

// Create a semi-transparent water texture
bgfx::TextureHandle create_water_texture() {
    std::cout << "Creating water texture..." << std::endl;
    
    const uint32_t textureWidth = 128;  // Lower resolution for water
    const uint32_t textureHeight = 128;
    const uint32_t textureSize = textureWidth * textureHeight * 4;
    
    const bgfx::Memory* texMem = bgfx::alloc(textureSize);
    uint8_t* data = texMem->data;
    
    // Create a semi-transparent blue water texture with subtle waves
    for (uint32_t y = 0; y < textureHeight; ++y) {
        for (uint32_t x = 0; x < textureWidth; ++x) {
            uint32_t offset = (y * textureWidth + x) * 4;
            
            // Create wave pattern
            float waveX = bx::sin(x * 0.1f) * 0.5f + 0.5f;
            float waveY = bx::cos(y * 0.08f) * 0.5f + 0.5f;
            float wave = waveX * waveY;
            
            // Base water color (blue-teal)
            uint8_t r = 40 + uint8_t(wave * 20);
            uint8_t g = 100 + uint8_t(wave * 30);
            uint8_t b = 160 + uint8_t(wave * 40);
            uint8_t a = 180; // Semi-transparent
            
            data[offset + 0] = r;
            data[offset + 1] = g;
            data[offset + 2] = b;
            data[offset + 3] = a;
        }
    }
    
    uint64_t textureFlags = BGFX_TEXTURE_NONE;
    textureFlags |= BGFX_SAMPLER_MIN_ANISOTROPIC;
    textureFlags |= BGFX_SAMPLER_MAG_ANISOTROPIC;
    
    return bgfx::createTexture2D(textureWidth, textureHeight, false, 1, bgfx::TextureFormat::RGBA8, textureFlags, texMem);
}

// Legacy function for single biome textures (kept for compatibility)
bgfx::TextureHandle create_biome_texture(BiomeType biome) {
    std::cout << "Creating " << (biome == BiomeType::DESERT ? "sand" : 
                                biome == BiomeType::GRASSLAND ? "light green" :
                                biome == BiomeType::SWAMP ? "dark green" : "brown-gray") 
              << " texture for biome..." << std::endl;
    
    const uint32_t textureWidth = 256;
    const uint32_t textureHeight = 256;
    const uint32_t textureSize = textureWidth * textureHeight * 4;
    
    const bgfx::Memory* texMem = bgfx::alloc(textureSize);
    uint8_t* data = texMem->data;
    
    // Base colors for each biome
    uint8_t baseR, baseG, baseB;
    uint8_t minR, maxR, minG, maxG, minB, maxB;
    
    switch (biome) {
        case BiomeType::DESERT:
            // Sandy colors
            baseR = 220; baseG = 185; baseB = 140;
            minR = 160; maxR = 240; minG = 130; maxG = 210; minB = 100; maxB = 170;
            break;
        case BiomeType::GRASSLAND:
            // Light green colors
            baseR = 100; baseG = 180; baseB = 80;
            minR = 80; maxR = 140; minG = 150; maxG = 220; minB = 60; maxB = 120;
            break;
        case BiomeType::SWAMP:
            // Dark green colors
            baseR = 60; baseG = 120; baseB = 50;
            minR = 40; maxR = 90; minG = 90; maxG = 150; minB = 30; maxB = 80;
            break;
        case BiomeType::MOUNTAINS:
            // Brown-gray colors
            baseR = 140; baseG = 120; baseB = 100;
            minR = 100; maxR = 180; minG = 90; maxG = 150; minB = 70; maxB = 130;
            break;
    }
    
    // Create grain patterns
    std::vector<std::vector<float>> grainPattern(textureWidth, std::vector<float>(textureHeight, 0.0f));
    
    int grainCount = (biome == BiomeType::SWAMP || biome == BiomeType::GRASSLAND) ? 150 : 200;
    
    for (int i = 0; i < grainCount; ++i) {
        int grainX = rand() % textureWidth;
        int grainY = rand() % textureHeight;
        int grainSize = 3 + (rand() % 4);
        float grainIntensity = 0.7f + (float)(rand() % 30) / 100.0f;
        
        for (int y = -grainSize; y <= grainSize; ++y) {
            for (int x = -grainSize; x <= grainSize; ++x) {
                int posX = grainX + x;
                int posY = grainY + y;
                
                if (posX >= 0 && posX < textureWidth && posY >= 0 && posY < textureHeight) {
                    float dist = std::sqrt(x*x + y*y);
                    if (dist <= grainSize) {
                        float effect = grainIntensity * (1.0f - dist/grainSize);
                        grainPattern[posX][posY] += effect;
                    }
                }
            }
        }
    }
    
    // Generate texture pixels with biome-specific patterns
    for (uint32_t y = 0; y < textureHeight; ++y) {
        for (uint32_t x = 0; x < textureWidth; ++x) {
            uint32_t offset = (y * textureWidth + x) * 4;
            
            uint8_t r = baseR, g = baseG, b = baseB;
            uint8_t microNoise = (uint8_t)((rand() % 20) - 10);
            
            float grainValue = bx::clamp(grainPattern[x][y], 0.0f, 1.0f);
            float darkening = 1.0f - (grainValue * 0.3f);
            
            // Biome-specific patterns
            float patternFactor = 1.0f;
            switch (biome) {
                case BiomeType::DESERT:
                    // Horizontal sand dune patterns
                    patternFactor = ((y / 12) % 2) == 0 ? 0.9f : 1.0f;
                    break;
                case BiomeType::GRASSLAND:
                    // Subtle grass blade patterns
                    patternFactor = ((x / 8 + y / 6) % 3) == 0 ? 0.95f : 1.0f;
                    break;
                case BiomeType::SWAMP:
                    // Wet, muddy texture with darker spots
                    patternFactor = ((x / 10 + y / 8) % 4) == 0 ? 0.8f : 1.0f;
                    break;
                case BiomeType::MOUNTAINS:
                    // Rocky, uneven texture
                    patternFactor = ((x / 6 + y / 9) % 3) == 0 ? 0.85f : 1.0f;
                    break;
            }
            
            r = (uint8_t)bx::clamp((r * darkening * patternFactor) + microNoise, (float)minR, (float)maxR);
            g = (uint8_t)bx::clamp((g * darkening * patternFactor) + microNoise, (float)minG, (float)maxG);
            b = (uint8_t)bx::clamp((b * darkening * patternFactor) + microNoise, (float)minB, (float)maxB);
            
            data[offset + 0] = r;
            data[offset + 1] = g;
            data[offset + 2] = b;
            data[offset + 3] = 255;
        }
    }
    
    uint64_t textureFlags = BGFX_TEXTURE_NONE;
    textureFlags |= BGFX_SAMPLER_MIN_ANISOTROPIC;
    textureFlags |= BGFX_SAMPLER_MAG_ANISOTROPIC;
    
    return bgfx::createTexture2D(textureWidth, textureHeight, false, 1, bgfx::TextureFormat::RGBA8, textureFlags, texMem);
}

// Create procedural texture (legacy function for backward compatibility)
bgfx::TextureHandle create_procedural_texture() {
    return create_biome_texture(BiomeType::DESERT); // Default to desert/sand texture
}

// Helper function to get SDL window native data
bool get_native_window_info(SDL_Window* window, void** native_window, void** native_display) {
#if BX_PLATFORM_OSX
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    *native_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    *native_display = nullptr;
    return *native_window != nullptr;
#elif BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    *native_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_WINDOW_POINTER, NULL);
    *native_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    return *native_window != nullptr;
#elif BX_PLATFORM_WINDOWS
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    *native_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    *native_display = nullptr;
    return *native_window != nullptr;
#else
    return false;
#endif
}

// Unified render object function
void render_object_at_position(bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, 
                              bgfx::ProgramHandle program, bgfx::TextureHandle texture, 
                              bgfx::UniformHandle texUniform, const float* modelMatrix) {
    bgfx::setTransform(modelMatrix);
    
    if (bgfx::isValid(texture) && bgfx::isValid(texUniform)) {
        bgfx::setTexture(0, texUniform, texture);
    }
    
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);
    
    // State is managed by caller now to support different render states
    // Default state is set by terrain and object rendering
    // Water rendering sets its own transparency state
    
    bgfx::submit(0, program);
}

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned int>(time(nullptr)));
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Initialize model system
    Model::init();
    
    // Create models
    Model gardenLampModel;
    
    // Create window
    std::cout << "Creating window..." << std::endl;
    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    std::cout << "Window created successfully!" << std::endl;

    // Get native window handle for BGFX
    void* native_window = nullptr;
    void* native_display = nullptr;
    if (!get_native_window_info(window, &native_window, &native_display)) {
        std::cerr << "Failed to get native window info!" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    std::cout << "Got native window handle: " << native_window << std::endl;

    // Initialize BGFX
    std::cout << "Initializing BGFX..." << std::endl;
    
    bgfx::renderFrame();
    
    bgfx::Init init;
    init.type = get_renderer_type();
    init.vendorId = BGFX_PCI_ID_NONE;
    init.platformData.nwh = native_window;
    init.platformData.ndt = native_display;
    init.resolution.width = WINDOW_WIDTH;
    init.resolution.height = WINDOW_HEIGHT;
    init.resolution.reset = BGFX_RESET_VSYNC;
    
    std::cout << "BGFX init parameters:" << std::endl;
    std::cout << "- Renderer type: " << (int)init.type << std::endl;
    std::cout << "- Window handle: " << init.platformData.nwh << std::endl;
    std::cout << "- Display handle: " << init.platformData.ndt << std::endl;
    std::cout << "- Resolution: " << init.resolution.width << "x" << init.resolution.height << std::endl;

    if (!bgfx::init(init)) {
        std::cerr << "Failed to initialize BGFX!" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    std::cout << "BGFX initialized successfully!" << std::endl;

    // Enable debug text for overlay
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    // Set view clear state
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    std::cout << "Preparing 3D rendering..." << std::endl;
    
    // Set up vertex layouts
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
        
    texLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    // Create vertex buffers
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), layout);
    
    bgfx::VertexBufferHandle copperVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(copperCubeVertices, sizeof(copperCubeVertices)), layout);
    
    bgfx::VertexBufferHandle ironVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(ironCubeVertices, sizeof(ironCubeVertices)), layout);
    
    bgfx::VertexBufferHandle stoneVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(stoneCubeVertices, sizeof(stoneCubeVertices)), layout);
    
    bgfx::VertexBufferHandle wandererVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(wandererCubeVertices, sizeof(wandererCubeVertices)), layout);
    
    bgfx::VertexBufferHandle villagerVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(villagerCubeVertices, sizeof(villagerCubeVertices)), layout);
    
    bgfx::VertexBufferHandle merchantVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(merchantCubeVertices, sizeof(merchantCubeVertices)), layout);
    
    bgfx::VertexBufferHandle texVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(texCubeVertices, sizeof(texCubeVertices)), texLayout);

    // Create index buffers
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(cubeIndices, sizeof(cubeIndices)));
    
    bgfx::IndexBufferHandle texIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(texCubeIndices, sizeof(texCubeIndices)));
    
    // Create textures
    bgfx::TextureHandle proceduralTexture = create_procedural_texture();
    bgfx::TextureHandle pngTexture = load_png_texture("assets/sandy_gravel_02_diff_1k.png");
    bgfx::TextureHandle waterTexture = create_water_texture();
    
    if (!bgfx::isValid(proceduralTexture)) {
        std::cerr << "Failed to create procedural texture!" << std::endl;
        return 1;
    }
    
    if (!bgfx::isValid(waterTexture)) {
        std::cerr << "Failed to create water texture!" << std::endl;
        return 1;
    }
    
    // Create texture uniforms
    bgfx::UniformHandle s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    
    // Load the Garden Lamp GLB model with detailed debugging
    std::cout << "Loading Garden Lamp GLB model with buffer debugging..." << std::endl;
    const char* modelPath = "assets/low-poly-garden-lamp-stylized-outdoor-light/source/garden lamp 1.glb";
    if (!gardenLampModel.loadFromFile(modelPath)) {
        std::cerr << "Failed to load Garden Lamp model!" << std::endl;
    } else {
        std::cout << "Garden Lamp model loaded successfully!" << std::endl;
        gardenLampModel.setFallbackTexture(proceduralTexture);
    }
    
    // Load shaders
    std::cout << "Loading shaders..." << std::endl;
    
    bgfx::ShaderHandle vsh, fsh, texVsh, texFsh;
    
#if BX_PLATFORM_OSX
    std::string vsPath = "shaders/metal/vs_cube.bin";
    std::string fsPath = "shaders/metal/fs_cube.bin";
    std::string texVsPath = "shaders/metal/vs_textured_cube.bin";
    std::string texFsPath = "shaders/metal/fs_textured_cube.bin";
    
    std::cout << "Looking for shaders at:" << std::endl;
    std::cout << "  Colored cube vertex: " << vsPath << std::endl;
    std::cout << "  Colored cube fragment: " << fsPath << std::endl;
    std::cout << "  Textured cube vertex: " << texVsPath << std::endl;
    std::cout << "  Textured cube fragment: " << texFsPath << std::endl;
    
    FILE* vsFile = fopen(vsPath.c_str(), "rb");
    FILE* fsFile = fopen(fsPath.c_str(), "rb");
    FILE* texVsFile = fopen(texVsPath.c_str(), "rb");
    FILE* texFsFile = fopen(texFsPath.c_str(), "rb");
    
    if (!vsFile || !fsFile || !texVsFile || !texFsFile) {
        if (vsFile) fclose(vsFile);
        if (fsFile) fclose(fsFile);
        if (texVsFile) fclose(texVsFile);
        if (texFsFile) fclose(texFsFile);
        std::cerr << "Failed to open shader files!" << std::endl;
        return 1;
    }
    
    // Get file sizes
    fseek(vsFile, 0, SEEK_END); long vsSize = ftell(vsFile); fseek(vsFile, 0, SEEK_SET);
    fseek(fsFile, 0, SEEK_END); long fsSize = ftell(fsFile); fseek(fsFile, 0, SEEK_SET);
    fseek(texVsFile, 0, SEEK_END); long texVsSize = ftell(texVsFile); fseek(texVsFile, 0, SEEK_SET);
    fseek(texFsFile, 0, SEEK_END); long texFsSize = ftell(texFsFile); fseek(texFsFile, 0, SEEK_SET);
    
    // Read the shader files
    const bgfx::Memory* vs_mem = bgfx::alloc(vsSize);
    const bgfx::Memory* fs_mem = bgfx::alloc(fsSize);
    const bgfx::Memory* texVs_mem = bgfx::alloc(texVsSize);
    const bgfx::Memory* texFs_mem = bgfx::alloc(texFsSize);
    
    size_t vsRead = fread(vs_mem->data, 1, vsSize, vsFile);
    size_t fsRead = fread(fs_mem->data, 1, fsSize, fsFile);
    size_t texVsRead = fread(texVs_mem->data, 1, texVsSize, texVsFile);
    size_t texFsRead = fread(texFs_mem->data, 1, texFsSize, texFsFile);
    
    fclose(vsFile); fclose(fsFile); fclose(texVsFile); fclose(texFsFile);
    
    if (vsRead != vsSize || fsRead != fsSize || texVsRead != texVsSize || texFsRead != texFsSize) {
        std::cerr << "Failed to read shader files fully!" << std::endl;
        return 1;
    }
    
    std::cout << "Creating shaders..." << std::endl;
    vsh = bgfx::createShader(vs_mem);
    fsh = bgfx::createShader(fs_mem);
    texVsh = bgfx::createShader(texVs_mem);
    texFsh = bgfx::createShader(texFs_mem);
    
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh) || !bgfx::isValid(texVsh) || !bgfx::isValid(texFsh)) {
        std::cerr << "Failed to create shader objects!" << std::endl;
        return 1;
    }
#else
    std::cerr << "Shaders for this platform are not implemented in this example." << std::endl;
    return 1;
#endif

    // Create shader programs
    std::cout << "Creating shader programs..." << std::endl;
    bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
    bgfx::ProgramHandle texProgram = bgfx::createProgram(texVsh, texFsh, true);
    
    if (!bgfx::isValid(program) || !bgfx::isValid(texProgram)) {
        std::cerr << "Failed to create shader programs!" << std::endl;
        return 1;
    }
    
    // Create chunk manager and player
    std::cout << "Creating chunk manager..." << std::endl;
    ChunkManager chunkManager;
    
    std::cout << "Creating player..." << std::endl;
    Player player;
    player.position = {0.0f, 0.0f, 0.0f}; // Will be set properly after first chunk load
    
    // Create player inventory
    PlayerInventory inventory;
    
    // Create debug overlay
    DebugOverlay debugOverlay;
    
    // Create resource nodes vector (will be populated by procedural generation)
    std::vector<ResourceNode> resourceNodes;
    
    // Create NPCs vector (will be populated by procedural generation)
    std::vector<NPC> npcs;
    
    // Connect ChunkManager to resource nodes and NPCs for procedural generation
    chunkManager.setResourceNodesPointer(&resourceNodes);
    chunkManager.setNPCsPointer(&npcs);
    
    // Force initial chunk loading around player (this will generate resources)
    chunkManager.forceInitialChunkLoad(player.position.x, player.position.z);
    player.position.y = chunkManager.getHeightAt(0.0f, 0.0f) + player.size - 5.0f;
    
    std::cout << "Initial world generation complete. Total resource nodes: " << resourceNodes.size() 
              << ", Total NPCs: " << npcs.size() << std::endl;
    std::cout << "Starting main loop..." << std::endl;
    std::cout << "===== Controls =====" << std::endl;
    std::cout << "WASD - Move camera" << std::endl;
    std::cout << "SHIFT - Sprint (faster movement)" << std::endl;
    std::cout << "Q/E  - Move up/down" << std::endl;
    std::cout << "1    - Jump to bird's eye view of player" << std::endl;
    std::cout << "Left click and drag - Rotate camera" << std::endl;
    std::cout << "Right click on NPC - Initiate combat (Kenshi-style)" << std::endl;
    std::cout << "Right click on terrain - Move player to location" << std::endl;
    std::cout << "Double right click - Sprint to location" << std::endl;
    std::cout << "SPACE - Mine nearby resource nodes" << std::endl;
    std::cout << "I    - Toggle inventory overlay" << std::endl;
    std::cout << "O    - Toggle debug overlay" << std::endl;
    std::cout << "H    - Test damage (health system)" << std::endl;
    std::cout << "J    - Test healing (health system)" << std::endl;
    std::cout << "ESC  - Exit" << std::endl;
    std::cout << "===================" << std::endl;
    std::cout << "Combat: NPCs will approach when you get close. Combat is automatic with RNG!" << std::endl;
    
    // Main game loop variables
    bool quit = false;
    SDL_Event event;
    float time = 0.0f;
    
    // Hover info system variables
    std::string hoverInfo = "";
    bool hasHoverInfo = false;
    
    // Camera system
    Camera camera;
    camera.setToPlayerBirdsEye(player);
    std::cout << "Initial camera set to bird's eye view of player" << std::endl;
    
    // UI system
    UIRenderer uiRenderer;
    if (!uiRenderer.init()) {
        std::cerr << "Failed to initialize UI renderer!" << std::endl;
        return 1;
    }
    
    // Mouse variables for terrain picking
    float pendingMouseX = 0.0f;
    float pendingMouseY = 0.0f;
    bool hasPendingClick = false;
    bool shouldSprint = false;
    
    // Double-click detection variables
    static const float DOUBLE_CLICK_TIME = 500.0f; // milliseconds
    Uint64 lastRightClickTime = 0;
    bool isDoubleClick = false;
    
    const bool* keyboardState = SDL_GetKeyboardState(NULL);
    
    // Main game loop
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    quit = true;
                }
                else if (event.key.key == SDLK_1) {
                    // Jump to bird's eye view of player
                    camera.setToPlayerBirdsEye(player);
                }
                else if (event.key.key == SDLK_O) {
                    // Toggle debug overlay
                    debugOverlay.toggle();
                }
                else if (event.key.key == SDLK_SPACE) {
                    // Mine nearby resource nodes
                    const float miningRange = 2.0f;
                    bool minedSomething = false;
                    
                    for (auto& node : resourceNodes) {
                        if (!node.canMine()) continue;
                        
                        float dx = player.position.x - node.position.x;
                        float dz = player.position.z - node.position.z;
                        float distance = bx::sqrt(dx * dx + dz * dz);
                        
                        if (distance <= miningRange) {
                            // Apply Mining skill modifier to damage
                            float miningModifier = player.skills.getSkill(SkillType::MINING).getModifier();
                            int miningDamage = (int)(25 * miningModifier);
                            
                            int resourceGained = node.mine(miningDamage);
                            if (resourceGained > 0) {
                                inventory.addResource(node.type, resourceGained);
                                // Award more Mining XP for depleting a node
                                player.skills.getSkill(SkillType::MINING).addExperience(10.0f);
                            } else {
                                // Award Mining XP for each mining action
                                player.skills.getSkill(SkillType::MINING).addExperience(2.0f);
                            }
                            minedSomething = true;
                            break; // Mine one node at a time
                        }
                    }
                    
                    if (!minedSomething) {
                        std::cout << "No resource nodes in range (need to be within " << miningRange << " units)" << std::endl;
                    }
                }
                else if (event.key.key == SDLK_I) {
                    // Toggle inventory overlay
                    std::cout << "I key pressed - toggling inventory..." << std::endl;
                    inventory.toggleOverlay();
                }
                else if (event.key.key == SDLK_H) {
                    // Test damage (H key for Health testing)
                    player.takeDamage(20, time);
                    std::cout << "H key pressed - test damage applied!" << std::endl;
                }
                else if (event.key.key == SDLK_J) {
                    // Test healing (J key for heal)
                    player.heal(25);
                    std::cout << "J key pressed - test healing applied!" << std::endl;
                }
                else if (event.key.key == SDLK_C) {
                    // Toggle skills overlay
                    std::cout << "C key pressed - toggling skills..." << std::endl;
                    player.skills.toggleOverlay();
                }
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                int width = event.window.data1;
                int height = event.window.data2;
                bgfx::reset(width, height, BGFX_RESET_VSYNC);
                bgfx::setViewRect(0, 0, 0, width, height);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    camera.handleMouseButton(event);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Right click for terrain picking with double-click detection
                    Uint64 currentTime = SDL_GetTicks();
                    
                    if (currentTime - lastRightClickTime < DOUBLE_CLICK_TIME) {
                        // Double-click detected
                        isDoubleClick = true;
                        shouldSprint = true;
                        std::cout << "Double right-click detected - Sprint mode!" << std::endl;
                    } else {
                        // Single click
                        isDoubleClick = false;
                        shouldSprint = false;
                    }
                    
                    lastRightClickTime = currentTime;
                    SDL_GetMouseState(&pendingMouseX, &pendingMouseY);
                    hasPendingClick = true;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                camera.handleMouseButton(event);
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                camera.handleMouseMotion(event);
            }
        }
        
        keyboardState = SDL_GetKeyboardState(NULL);
        
        time += 0.01f;
        float deltaTime = 0.01f; // Fixed delta time for now
        
        // Update camera with keyboard input
        camera.handleKeyboardInput(keyboardState, deltaTime);
        
        // Set up camera view matrix
        float view[16];
        camera.getViewMatrix(view);
        
        float proj[16];
        bx::mtxProj(proj, 60.0f, float(WINDOW_WIDTH) / float(WINDOW_HEIGHT), 0.1f, 100.0f, 
                   bgfx::getCaps()->homogeneousDepth);
        
        bgfx::setViewTransform(0, view, proj);
        bgfx::setViewRect(0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        // Get current window size for various uses
        int currentWidth, currentHeight;
        SDL_GetWindowSize(window, &currentWidth, &currentHeight);
        
        // Handle right-click picking (NPC first, then terrain)
        if (hasPendingClick) {
            std::cout << "Right-click detected! Window size: " << currentWidth << "x" << currentHeight << std::endl;
            Ray ray = createRayFromMouse(pendingMouseX, pendingMouseY, currentWidth, currentHeight, view, proj);
            
            // First check if we clicked on an NPC
            float closestNPCDist = FLT_MAX;
            NPC* clickedNPC = nullptr;
            
            for (auto& npc : npcs) {
                if (!npc.isActive) continue;
                
                // Ray-sphere intersection test
                bx::Vec3 toNPC = {
                    npc.position.x - ray.origin.x,
                    npc.position.y - ray.origin.y,
                    npc.position.z - ray.origin.z
                };
                
                float projDist = bx::dot(toNPC, ray.direction);
                if (projDist < 0) continue; // Behind camera
                
                bx::Vec3 closestPoint = {
                    ray.origin.x + ray.direction.x * projDist,
                    ray.origin.y + ray.direction.y * projDist,
                    ray.origin.z + ray.direction.z * projDist
                };
                
                float dx = closestPoint.x - npc.position.x;
                float dy = closestPoint.y - npc.position.y;
                float dz = closestPoint.z - npc.position.z;
                float distToNPC = bx::sqrt(dx*dx + dy*dy + dz*dz);
                
                // Check if within NPC's bounding sphere
                if (distToNPC <= npc.size * 1.5f && projDist < closestNPCDist) {
                    closestNPCDist = projDist;
                    clickedNPC = &npc;
                }
            }
            
            if (clickedNPC) {
                // Clicked on an NPC - set as combat target and move toward them
                player.combatTarget = clickedNPC;
                clickedNPC->combatTarget = &player;
                clickedNPC->isHostile = true;
                
                // Set movement target to NPC position so player moves toward them
                player.setTarget(clickedNPC->position.x, clickedNPC->position.z, chunkManager, shouldSprint);
                
                std::cout << "Targeting " << clickedNPC->getTypeName() << " for combat!" << std::endl;
            } else {
                // No NPC clicked, check terrain
                bx::Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
                if (rayTerrainIntersection(ray, chunkManager, hitPoint)) {
                    // Clear combat target when moving to terrain
                    player.combatTarget = nullptr;
                    player.setTarget(hitPoint.x, hitPoint.z, chunkManager, shouldSprint);
                    if (shouldSprint) {
                        std::cout << "Player sprinting to: (" << hitPoint.x << ", " << hitPoint.z << ")" << std::endl;
                    } else {
                        std::cout << "Player moving to: (" << hitPoint.x << ", " << hitPoint.z << ")" << std::endl;
                    }
                } else {
                    std::cout << "No terrain intersection found!" << std::endl;
                }
            }
            
            hasPendingClick = false;
            shouldSprint = false; // Reset sprint flag
        }
        
        // Update debug overlay
        debugOverlay.update();
        
        // Update player and chunks
        player.update(chunkManager, time, deltaTime);
        chunkManager.updateChunksAroundPlayer(player.position.x, player.position.z);
        
        // Check for hover over objects
        hasHoverInfo = false;
        hoverInfo = "";
        
        // Get current mouse position
        float currentMouseX, currentMouseY;
        SDL_GetMouseState(&currentMouseX, &currentMouseY);
        
        // Create ray from mouse position
        Ray hoverRay = createRayFromMouse(currentMouseX, currentMouseY, currentWidth, currentHeight, view, proj);
        
        // Check NPCs for hover
        float closestNPCDist = FLT_MAX;
        NPC* hoveredNPC = nullptr;
        
        for (auto& npc : npcs) {
            if (!npc.isActive) continue;
            
            // Simple ray-sphere intersection test
            bx::Vec3 toNPC = {
                npc.position.x - hoverRay.origin.x,
                npc.position.y - hoverRay.origin.y,
                npc.position.z - hoverRay.origin.z
            };
            
            float projDist = bx::dot(toNPC, hoverRay.direction);
            if (projDist < 0) continue; // Behind camera
            
            bx::Vec3 closestPoint = {
                hoverRay.origin.x + hoverRay.direction.x * projDist,
                hoverRay.origin.y + hoverRay.direction.y * projDist,
                hoverRay.origin.z + hoverRay.direction.z * projDist
            };
            
            float dx = closestPoint.x - npc.position.x;
            float dy = closestPoint.y - npc.position.y;
            float dz = closestPoint.z - npc.position.z;
            float distToNPC = bx::sqrt(dx*dx + dy*dy + dz*dz);
            
            // Check if within NPC's bounding sphere (size * 2 for easier hovering)
            if (distToNPC <= npc.size * 2.0f && projDist < closestNPCDist) {
                closestNPCDist = projDist;
                hoveredNPC = &npc;
            }
        }
        
        // Check resources for hover (only if no NPC is hovered)
        if (!hoveredNPC) {
            float closestResourceDist = FLT_MAX;
            ResourceNode* hoveredResource = nullptr;
            
            for (auto& node : resourceNodes) {
                if (!node.isActive) continue;
                
                // Simple ray-sphere intersection test
                bx::Vec3 toResource = {
                    node.position.x - hoverRay.origin.x,
                    node.position.y - hoverRay.origin.y,
                    node.position.z - hoverRay.origin.z
                };
                
                float projDist = bx::dot(toResource, hoverRay.direction);
                if (projDist < 0) continue; // Behind camera
                
                bx::Vec3 closestPoint = {
                    hoverRay.origin.x + hoverRay.direction.x * projDist,
                    hoverRay.origin.y + hoverRay.direction.y * projDist,
                    hoverRay.origin.z + hoverRay.direction.z * projDist
                };
                
                float dx = closestPoint.x - node.position.x;
                float dy = closestPoint.y - node.position.y;
                float dz = closestPoint.z - node.position.z;
                float distToResource = bx::sqrt(dx*dx + dy*dy + dz*dz);
                
                // Check if within resource's bounding sphere (size * 2 for easier hovering)
                if (distToResource <= node.size * 2.0f && projDist < closestResourceDist) {
                    closestResourceDist = projDist;
                    hoveredResource = &node;
                }
            }
            
            if (hoveredResource) {
                // Format resource hover info
                hoverInfo = hoveredResource->getResourceName();
                hasHoverInfo = true;
            }
        } else {
            // Format NPC hover info
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s - Health: %d/%d", 
                     hoveredNPC->getTypeName(), hoveredNPC->health, hoveredNPC->maxHealth);
            hoverInfo = buffer;
            hasHoverInfo = true;
        }
        
        // Render all loaded terrain chunks
        chunkManager.renderChunks(texProgram, s_texColor);
        
        // Render water with transparency enabled
        chunkManager.renderWater(texProgram, s_texColor, waterTexture);
        
        // Render player as a colored cube
        float playerMatrix[16], playerTranslation[16], playerScale[16];
        bx::mtxScale(playerScale, player.size, player.size, player.size);
        bx::mtxTranslate(playerTranslation, player.position.x, player.position.y, player.position.z);
        bx::mtxMul(playerMatrix, playerScale, playerTranslation);
        
        // Set default state for objects
        uint64_t objState = BGFX_STATE_DEFAULT;
        objState &= ~BGFX_STATE_CULL_MASK;
        bgfx::setState(objState);
        
        // Create player vertices with hit flash
        if (player.hitFlashTimer > 0) {
            PosColorVertex playerVertices[8];
            // Calculate flash intensity
            float flashIntensity = player.hitFlashTimer / 0.2f;
            uint32_t baseColor = 0xffffffff; // White player
            uint32_t r = 255;
            uint32_t g = 255 * (1.0f - flashIntensity * 0.5f);
            uint32_t b = 255 * (1.0f - flashIntensity * 0.8f);
            uint32_t flashColor = 0xff000000 | (b << 16) | (g << 8) | r; // ABGR format
            
            for (int i = 0; i < 8; i++) {
                playerVertices[i] = cubeVertices[i];
                playerVertices[i].abgr = flashColor;
            }
            
            // Create transient vertex buffer
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, 8, layout);
            bx::memCopy(tvb.data, playerVertices, sizeof(playerVertices));
            
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(ibh);
            bgfx::setState(objState);
            bgfx::setTransform(playerMatrix);
            bgfx::submit(0, program);
        } else {
            render_object_at_position(vbh, ibh, program, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, playerMatrix);
        }
        
        // Render resource nodes
        for (const auto& node : resourceNodes) {
            if (!node.isActive) continue; // Don't render depleted nodes
            
            float nodeMatrix[16], nodeTranslation[16], nodeScale[16];
            bx::mtxScale(nodeScale, node.size, node.size, node.size);
            bx::mtxTranslate(nodeTranslation, node.position.x, node.position.y, node.position.z);
            bx::mtxMul(nodeMatrix, nodeScale, nodeTranslation);
            
            // Use appropriate colored vertex buffer for each resource type
            bgfx::VertexBufferHandle nodeVbh;
            switch (node.type) {
                case ResourceType::COPPER:
                    nodeVbh = copperVbh;
                    break;
                case ResourceType::IRON:
                    nodeVbh = ironVbh;
                    break;
                case ResourceType::STONE:
                    nodeVbh = stoneVbh;
                    break;
                default:
                    nodeVbh = vbh; // Fallback to regular colored cube
                    break;
            }
            render_object_at_position(nodeVbh, ibh, program, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, nodeMatrix);
        }
        
        // Update and render NPCs
        for (auto& npc : npcs) {
            if (!npc.isActive) continue;
            
            // Update NPC AI
            float npcTerrainHeight = chunkManager.getHeightAt(npc.position.x, npc.position.z);
            npc.update(deltaTime, npcTerrainHeight, &player, time);
            npc.updateHealthColor(); // Ensure color is updated for flash effect
            
            // Render NPC
            float npcMatrix[16], npcTranslation[16], npcScale[16];
            bx::mtxScale(npcScale, npc.size, npc.size, npc.size);
            bx::mtxTranslate(npcTranslation, npc.position.x, npc.position.y, npc.position.z);
            bx::mtxMul(npcMatrix, npcScale, npcTranslation);
            
            // Create dynamic vertex buffer with current color
            PosColorVertex npcVertices[8];
            
            // Copy base vertex data from appropriate type
            const PosColorVertex* baseVertices = nullptr;
            switch (npc.type) {
                case NPCType::WANDERER:
                    baseVertices = wandererCubeVertices;
                    break;
                case NPCType::VILLAGER:
                    baseVertices = villagerCubeVertices;
                    break;
                case NPCType::MERCHANT:
                    baseVertices = merchantCubeVertices;
                    break;
                default:
                    baseVertices = wandererCubeVertices;
                    break;
            }
            
            // Apply hit flash color or health-based color
            for (int i = 0; i < 8; i++) {
                npcVertices[i] = baseVertices[i];
                // Override color with dynamic color (includes hit flash)
                npcVertices[i].abgr = npc.color;
            }
            
            // Create transient vertex buffer with current color
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, 8, layout);
            bx::memCopy(tvb.data, npcVertices, sizeof(npcVertices));
            
            // Set vertex buffer and render
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(ibh);
            bgfx::setState(objState);
            bgfx::setTransform(npcMatrix);
            bgfx::submit(0, program);
        }
        
        // Set state for test cubes (with culling disabled for spinning cubes)
        uint64_t testCubeState = BGFX_STATE_DEFAULT;
        testCubeState &= ~BGFX_STATE_CULL_MASK;
        bgfx::setState(testCubeState);
        
        // Render colored cube (left side)
        float coloredMtx[16], translation[16], rotation[16];
        bx::mtxTranslate(translation, -2.5f, 0.0f, 0.0f);
        bx::mtxRotateXY(rotation, time * 0.21f, time * 0.37f);
        bx::mtxMul(coloredMtx, rotation, translation);
        render_object_at_position(vbh, ibh, program, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, coloredMtx);
        
        // Render textured cube (right side)
        float texturedMtx[16];
        bx::mtxTranslate(translation, 2.5f, 0.0f, 0.0f);
        bx::mtxRotateXY(rotation, time * -0.21f, time * -0.37f);
        bx::mtxMul(texturedMtx, rotation, translation);
        bgfx::setState(testCubeState);
        render_object_at_position(texVbh, texIbh, texProgram, proceduralTexture, s_texColor, texturedMtx);
        
        // Render PNG textured cube (top) if available
        if (bgfx::isValid(pngTexture)) {
            float pngTexMtx[16];
            bx::mtxTranslate(translation, 0.0f, 2.5f, 0.0f);
            bx::mtxRotateXY(rotation, time * 0.15f, time * 0.3f);
            bx::mtxMul(pngTexMtx, rotation, translation);
            bgfx::setState(testCubeState);
            render_object_at_position(texVbh, texIbh, texProgram, pngTexture, s_texColor, pngTexMtx);
        }
        
        // Render the Garden Lamp model with debugging
        if (gardenLampModel.hasAnyMeshes()) {
            float modelMatrix[16], scale[16], temp[16];
            bx::mtxIdentity(modelMatrix);
            bx::mtxScale(scale, 2.0f, 2.0f, 2.0f);  // Reasonable scale
            bx::mtxRotateY(rotation, time * 0.5f);
            bx::mtxTranslate(translation, 0.0f, -1.0f, 0.0f);  // Slightly below center
            
            bx::mtxMul(temp, scale, rotation);
            bx::mtxMul(modelMatrix, temp, translation);
            
            bgfx::setState(testCubeState);
            gardenLampModel.render(texProgram, s_texColor, modelMatrix);
        }
        
        // UI system is now working! Test code removed.
        
        // Start UI rendering
        uiRenderer.begin(currentWidth, currentHeight);
        
        // Render debug overlay if enabled
        if (debugOverlay.enabled) {
            // Create debug panel background (black with transparency) - ABGR format - taller panel
            uiRenderer.panel(currentWidth - 220, 10, 210, 110, 0xAA000000); // Black with 66% alpha (ABGR)
            
            // Render FPS and debug info with custom UI - better line spacing for 24px font
            char fpsText[64];
            snprintf(fpsText, sizeof(fpsText), "FPS: %3d (%4.1fms)", (int)debugOverlay.fps, debugOverlay.frameTime);
            uiRenderer.text(currentWidth - 210, 25, fpsText, UIColors::TEXT_NORMAL);
            
            snprintf(fpsText, sizeof(fpsText), "Chunks: %d", (int)chunkManager.getLoadedChunkInfo().size());
            uiRenderer.text(currentWidth - 210, 55, fpsText, UIColors::TEXT_NORMAL);  // 30px spacing instead of 20px
            
            snprintf(fpsText, sizeof(fpsText), "Player: %.1f,%.1f", player.position.x, player.position.z);
            uiRenderer.text(currentWidth - 210, 85, fpsText, UIColors::TEXT_NORMAL);  // 30px spacing instead of 20px
        }
        
        // Render inventory overlay if enabled
        inventory.renderOverlay(uiRenderer);
        
        // Render skills overlay if enabled
        player.skills.renderOverlay(uiRenderer, currentHeight);
        
        // Render player health bar (always visible)
        player.renderHealthBar(uiRenderer, currentWidth);
        
        // Render hover info if available (centered at top)
        if (hasHoverInfo && !hoverInfo.empty()) {
            uiRenderer.textCentered(currentWidth / 2, 50, hoverInfo.c_str(), UIColors::TEXT_HIGHLIGHT);
        }
        
        // End UI rendering
        uiRenderer.end();
        
        bgfx::frame();
    }
    
    // Clean up resources
    uiRenderer.destroy();
    bgfx::destroy(proceduralTexture);
    if (bgfx::isValid(pngTexture)) bgfx::destroy(pngTexture);
    bgfx::destroy(waterTexture);
    bgfx::destroy(s_texColor);
    bgfx::destroy(ibh);
    bgfx::destroy(vbh);
    bgfx::destroy(copperVbh);
    bgfx::destroy(ironVbh);
    bgfx::destroy(stoneVbh);
    bgfx::destroy(wandererVbh);
    bgfx::destroy(villagerVbh);
    bgfx::destroy(merchantVbh);
    bgfx::destroy(texIbh);
    bgfx::destroy(texVbh);
    bgfx::destroy(program);
    bgfx::destroy(texProgram);
    bgfx::destroy(vsh);
    bgfx::destroy(fsh);
    bgfx::destroy(texVsh);
    bgfx::destroy(texFsh);
    
    gardenLampModel.unload();
    
    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}