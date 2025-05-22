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

// Window dimensions
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const char* WINDOW_TITLE = "My First C++ Game with BGFX";

// Colors
const uint32_t CLEAR_COLOR = 0x303030ff;

// Camera settings
const float CAMERA_MOVE_SPEED = 0.1f;
const float CAMERA_SPRINT_MULTIPLIER = 3.0f;
const float CAMERA_ROTATE_SPEED = 0.005f;

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

// NPC cube vertices (will be dynamically colored)
static PosColorVertex npcCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff00AA00}, // 0: Front-top-left (will be set per NPC)
    { 1.0f,  1.0f,  1.0f, 0xff00AA00}, // 1: Front-top-right
    {-1.0f, -1.0f,  1.0f, 0xff008800}, // 2: Front-bottom-left (darker variant)
    { 1.0f, -1.0f,  1.0f, 0xff008800}, // 3: Front-bottom-right (darker variant)
    {-1.0f,  1.0f, -1.0f, 0xff00CC00}, // 4: Back-top-left (lighter variant)
    { 1.0f,  1.0f, -1.0f, 0xff00CC00}, // 5: Back-top-right (lighter variant)
    {-1.0f, -1.0f, -1.0f, 0xff008800}, // 6: Back-bottom-left (darker variant)
    { 1.0f, -1.0f, -1.0f, 0xff008800}  // 7: Back-bottom-right (darker variant)
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
    
    TerrainChunk(int cx, int cz, BiomeType biomeType) 
        : chunkX(cx), chunkZ(cz), biome(biomeType) {
        vbh = BGFX_INVALID_HANDLE;
        ibh = BGFX_INVALID_HANDLE;
        texture = BGFX_INVALID_HANDLE;
    }
    
    ~TerrainChunk() {
        if (bgfx::isValid(vbh)) bgfx::destroy(vbh);
        if (bgfx::isValid(ibh)) bgfx::destroy(ibh);
        if (bgfx::isValid(texture)) bgfx::destroy(texture);
    }
    
    void generate() {
        vertices.clear();
        indices.clear();
        
        // Generate vertices with biome-specific terrain
        generateBiomeTerrain();
        generateIndices();
        
        // Validate chunk geometry
        validateChunkGeometry();
        
        std::cout << "Generated " << getBiomeName() << " chunk (" << chunkX << ", " << chunkZ 
                  << ") with " << vertices.size() << " vertices" << std::endl;
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
        float swampHeight = baseNoise * HEIGHT_SCALE * 0.4f + detailNoise2 * HEIGHT_SCALE * 0.15f + fineNoise * HEIGHT_SCALE * 0.05f - 1.0f;
        
        float desertHeight = baseNoise * HEIGHT_SCALE * 1.2f + detailNoise1 * HEIGHT_SCALE * 0.3f + fineNoise * HEIGHT_SCALE * 0.1f;
        
        float grasslandHeight = baseNoise * HEIGHT_SCALE * 1.5f + detailNoise1 * HEIGHT_SCALE * 0.8f + detailNoise2 * HEIGHT_SCALE * 0.4f + fineNoise * HEIGHT_SCALE * 0.2f;
        
        float mountainHeight = (baseNoise + 0.2f) * HEIGHT_SCALE * 2.5f + detailNoise1 * HEIGHT_SCALE * 1.2f + detailNoise2 * HEIGHT_SCALE * 0.6f + fineNoise * HEIGHT_SCALE * 0.3f + 8.0f;
        
        // Blend heights based on biome weights for seamless transitions
        float blendedHeight = swampHeight * swampWeight + desertHeight * desertWeight + grasslandHeight * grasslandWeight + mountainHeight * mountainWeight;
        
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
enum class ResourceType {
    COPPER,
    IRON,
    STONE
};

struct ResourceNode {
    bx::Vec3 position;
    ResourceType type;
    int health;
    int maxHealth;
    float size;
    bool isActive;
    
    ResourceNode(float x, float y, float z, ResourceType resourceType, int hp = 100) 
        : position({x, y, z}), type(resourceType), health(hp), maxHealth(hp), size(0.5f), isActive(true) {}
    
    bool canMine() const {
        return isActive && health > 0;
    }
    
    int mine(int damage = 25) {
        if (!canMine()) return 0;
        
        health -= damage;
        std::cout << "Mining " << getResourceName() << " - Health: " << health << "/" << maxHealth << std::endl;
        
        if (health <= 0) {
            health = 0;
            isActive = false;
            std::cout << getResourceName() << " node depleted! Gained 1 " << getResourceName() << std::endl;
            return 1; // Return resource gained
        }
        return 0;
    }
    
    const char* getResourceName() const {
        switch (type) {
            case ResourceType::COPPER: return "Copper";
            case ResourceType::IRON: return "Iron";
            case ResourceType::STONE: return "Stone";
            default: return "Unknown";
        }
    }
    
    uint32_t getColor() const {
        switch (type) {
            case ResourceType::COPPER: return 0xff4A90E2; // Orange-brown for copper
            case ResourceType::IRON: return 0xff808080;   // Gray for iron
            case ResourceType::STONE: return 0xff606060;  // Dark gray for stone
            default: return 0xffffffff;
        }
    }
};

// NPC System
enum class NPCType {
    WANDERER,
    VILLAGER,
    MERCHANT
};

enum class NPCState {
    WANDERING,
    IDLE,
    MOVING_TO_TARGET
};

struct NPC {
    bx::Vec3 position;
    bx::Vec3 velocity;
    bx::Vec3 targetPosition;
    NPCType type;
    NPCState state;
    float speed;
    float size;
    float stateTimer;
    float maxStateTime;
    bool isActive;
    uint32_t color;
    uint32_t baseColor;      // Original color for health calculation
    int health;
    int maxHealth;
    bool isHostile;
    float lastDamageTime;
    
    NPC(float x, float y, float z, NPCType npcType) 
        : position({x, y, z}), velocity({0.0f, 0.0f, 0.0f}), targetPosition({x, y, z}),
          type(npcType), state(NPCState::IDLE), speed(1.5f), size(0.8f), 
          stateTimer(0.0f), maxStateTime(3.0f), isActive(true), isHostile(false), lastDamageTime(0.0f) {
        
        // Set NPC-specific properties
        switch (type) {
            case NPCType::WANDERER:
                speed = 2.0f;
                baseColor = 0xff00AA00; // Green
                maxHealth = 60;         // Medium health
                maxStateTime = 2.0f + (bx::sin(x * 0.1f) * 0.5f + 0.5f) * 3.0f; // 2-5 seconds
                break;
            case NPCType::VILLAGER:
                speed = 1.0f;
                baseColor = 0xff0066FF; // Blue  
                maxHealth = 40;         // Low health (peaceful)
                maxStateTime = 4.0f + (bx::cos(z * 0.1f) * 0.5f + 0.5f) * 4.0f; // 4-8 seconds
                break;
            case NPCType::MERCHANT:
                speed = 1.5f;
                baseColor = 0xffFFAA00; // Orange
                maxHealth = 80;         // High health (well-armed traders)
                maxStateTime = 3.0f + (bx::sin((x + z) * 0.1f) * 0.5f + 0.5f) * 2.0f; // 3-5 seconds
                break;
        }
        
        health = maxHealth;  // Start at full health
        color = baseColor;   // Start with base color
        updateHealthColor(); // Apply initial color
    }
    
    void update(float deltaTime, float terrainHeight) {
        if (!isActive) return;
        
        stateTimer += deltaTime;
        
        switch (state) {
            case NPCState::IDLE:
                if (stateTimer >= maxStateTime) {
                    // Pick a new random target within reasonable distance
                    float angle = (bx::sin(position.x * 0.7f + stateTimer) * 0.5f + 0.5f) * 2.0f * bx::kPi;
                    float distance = 5.0f + (bx::cos(position.z * 0.8f + stateTimer) * 0.5f + 0.5f) * 10.0f; // 5-15 units
                    
                    targetPosition.x = position.x + bx::cos(angle) * distance;
                    targetPosition.z = position.z + bx::sin(angle) * distance;
                    
                    state = NPCState::MOVING_TO_TARGET;
                    stateTimer = 0.0f;
                }
                break;
                
            case NPCState::MOVING_TO_TARGET: {
                // Move towards target
                float dx = targetPosition.x - position.x;
                float dz = targetPosition.z - position.z;
                float distance = bx::sqrt(dx * dx + dz * dz);
                
                if (distance < 1.0f) {
                    // Reached target, go idle
                    state = NPCState::IDLE;
                    stateTimer = 0.0f;
                    velocity = {0.0f, 0.0f, 0.0f};
                } else {
                    // Move towards target
                    velocity.x = (dx / distance) * speed;
                    velocity.z = (dz / distance) * speed;
                    
                    position.x += velocity.x * deltaTime;
                    position.z += velocity.z * deltaTime;
                    
                    // Update Y position to follow terrain
                    position.y = terrainHeight + size - 5.0f;
                }
                
                // Timeout check - if moving too long, go idle
                if (stateTimer > 15.0f) {
                    state = NPCState::IDLE;
                    stateTimer = 0.0f;
                    velocity = {0.0f, 0.0f, 0.0f};
                }
                break;
            }
                
            case NPCState::WANDERING:
                // Continuous random walk
                if (stateTimer >= 1.0f) {
                    float angle = (bx::sin(position.x * 1.1f + stateTimer) + bx::cos(position.z * 0.9f + stateTimer)) * 0.5f;
                    velocity.x = bx::cos(angle) * speed * 0.5f;
                    velocity.z = bx::sin(angle) * speed * 0.5f;
                    stateTimer = 0.0f;
                }
                
                position.x += velocity.x * deltaTime;
                position.z += velocity.z * deltaTime;
                
                // Update Y position to follow terrain
                position.y = terrainHeight + size - 5.0f;
                break;
        }
    }
    
    const char* getTypeName() const {
        switch (type) {
            case NPCType::WANDERER: return "Wanderer";
            case NPCType::VILLAGER: return "Villager";
            case NPCType::MERCHANT: return "Merchant";
            default: return "Unknown";
        }
    }
    
    void takeDamage(int damage, float currentTime) {
        health = bx::max(0, health - damage);
        lastDamageTime = currentTime;
        updateHealthColor();
        
        // Make NPC hostile when attacked (except villagers who flee)
        if (type != NPCType::VILLAGER) {
            isHostile = true;
        }
        
        std::cout << getTypeName() << " took " << damage << " damage! Health: " << health << "/" << maxHealth;
        if (health <= 0) {
            std::cout << " - " << getTypeName() << " defeated!";
            isActive = false;
        }
        std::cout << std::endl;
    }
    
    void updateHealthColor() {
        if (health <= 0) {
            color = 0xff404040; // Dark gray when dead
            return;
        }
        
        float healthPercent = (float)health / (float)maxHealth;
        
        if (isHostile) {
            // Hostile NPCs get red tint
            if (healthPercent > 0.7f) color = 0xffAA0000; // Dark red
            else if (healthPercent > 0.3f) color = 0xff880000; // Darker red
            else color = 0xff660000; // Very dark red
        } else {
            // Damaged but peaceful NPCs get darker versions of base color
            if (healthPercent > 0.7f) {
                color = baseColor; // Full color
            } else if (healthPercent > 0.3f) {
                // 50% darker
                uint32_t r = ((baseColor >> 16) & 0xFF) / 2;
                uint32_t g = ((baseColor >> 8) & 0xFF) / 2;
                uint32_t b = (baseColor & 0xFF) / 2;
                color = 0xff000000 | (r << 16) | (g << 8) | b;
            } else {
                // 75% darker (very dark)
                uint32_t r = ((baseColor >> 16) & 0xFF) / 4;
                uint32_t g = ((baseColor >> 8) & 0xFF) / 4;
                uint32_t b = (baseColor & 0xFF) / 4;
                color = 0xff000000 | (r << 16) | (g << 8) | b;
            }
        }
    }
    
    bool canTakeDamage(float currentTime) const {
        return (currentTime - lastDamageTime) > 0.5f; // 0.5 second damage immunity
    }
    
    void heal(int amount) {
        health = bx::min(maxHealth, health + amount);
        updateHealthColor();
        std::cout << getTypeName() << " healed " << amount << " HP! Health: " << health << "/" << maxHealth << std::endl;
    }
};

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

// Player system
struct Player {
    bx::Vec3 position;
    bx::Vec3 targetPosition;
    bool hasTarget;
    bool isSprinting;
    float moveSpeed;
    float sprintSpeed;
    float size;
    int health;
    int maxHealth;
    float lastDamageTime;
    
    Player() : position({0.0f, 0.0f, 0.0f}), targetPosition({0.0f, 0.0f, 0.0f}), 
               hasTarget(false), isSprinting(false), moveSpeed(0.05f), sprintSpeed(0.15f), size(0.3f),
               health(100), maxHealth(100), lastDamageTime(0.0f) {}
    
    void setTarget(float x, float z, const ChunkManager& chunkManager, bool sprint = false) {
        targetPosition.x = x;
        targetPosition.z = z;
        targetPosition.y = chunkManager.getHeightAt(x, z) + size - 5.0f; // Account for terrain offset
        hasTarget = true;
        isSprinting = sprint;
    }
    
    void update(const ChunkManager& chunkManager) {
        if (hasTarget) {
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
                
                float currentSpeed = isSprinting ? sprintSpeed : moveSpeed;
                position.x += direction.x * currentSpeed;
                position.z += direction.z * currentSpeed;
                position.y = chunkManager.getHeightAt(position.x, position.z) + size - 5.0f; // Account for terrain offset
            }
        }
    }
    
    void takeDamage(int damage, float currentTime) {
        if (canTakeDamage(currentTime)) {
            health = bx::max(0, health - damage);
            lastDamageTime = currentTime;
            std::cout << "Player took " << damage << " damage! Health: " << health << "/" << maxHealth << std::endl;
            
            if (health <= 0) {
                std::cout << "Player defeated! Respawning..." << std::endl;
                respawn();
            }
        }
    }
    
    bool canTakeDamage(float currentTime) const {
        return (currentTime - lastDamageTime) > 1.0f; // 1 second damage immunity
    }
    
    void heal(int amount) {
        health = bx::min(maxHealth, health + amount);
        std::cout << "Player healed " << amount << " HP! Health: " << health << "/" << maxHealth << std::endl;
    }
    
    void respawn() {
        health = maxHealth;
        position = {0.0f, 0.0f, 0.0f}; // Reset to spawn point
        hasTarget = false;
        lastDamageTime = 0.0f;
    }
    
    void renderHealthBar() const {
        // Calculate health percentage for color coding
        float healthPercent = (float)health / (float)maxHealth;
        
        // Choose color based on health percentage
        uint32_t healthColor;
        if (healthPercent > 0.7f) healthColor = 0x0a; // Green
        else if (healthPercent > 0.3f) healthColor = 0x0e; // Yellow  
        else healthColor = 0x0c; // Red
        
        // Position in top-middle of screen (assuming 80 character width)
        int centerX = 35; // Approximate center for "Health: 100/100"
        int topY = 1;     // Top row
        
        bgfx::dbgTextPrintf(centerX, topY, healthColor, "Health: %d/%d", health, maxHealth);
    }
};

// Player inventory system
struct PlayerInventory {
    int copper = 0;
    int iron = 0;
    int stone = 0;
    bool showOverlay = false;
    
    void addResource(ResourceType type, int amount) {
        switch (type) {
            case ResourceType::COPPER: copper += amount; break;
            case ResourceType::IRON: iron += amount; break;
            case ResourceType::STONE: stone += amount; break;
        }
        printInventory();
    }
    
    void printInventory() const {
        std::cout << "=== INVENTORY ===" << std::endl;
        std::cout << "Copper: " << copper << std::endl;
        std::cout << "Iron: " << iron << std::endl;
        std::cout << "Stone: " << stone << std::endl;
        std::cout << "=================" << std::endl;
    }
    
    void toggleOverlay() {
        showOverlay = !showOverlay;
        std::cout << "Inventory overlay " << (showOverlay ? "enabled" : "disabled") << std::endl;
    }
    
    void renderOverlay() const {
        if (!showOverlay) return;
        
        // Position inventory in top left corner
        int x = 1; // 1 character from left edge
        int y = 1; // 1 character from top edge
        
        // Render inventory header
        bgfx::dbgTextPrintf(x, y, 0x0f, "=== INVENTORY ===");
        
        // Render resource counts with color coding
        bgfx::dbgTextPrintf(x, y + 1, 0x06, "Copper: %d", copper);  // Orange/brown color
        bgfx::dbgTextPrintf(x, y + 2, 0x08, "Iron:   %d", iron);    // Gray color
        bgfx::dbgTextPrintf(x, y + 3, 0x07, "Stone:  %d", stone);   // Light gray color
        
        // Render footer
        bgfx::dbgTextPrintf(x, y + 4, 0x0f, "=================");
        bgfx::dbgTextPrintf(x, y + 5, 0x0a, "Press I to close");    // Green color for instruction
    }
};

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
    
    std::cout << "Mouse: (" << mouseX << ", " << mouseY << ") -> NDC: (" << mouseXNDC << ", " << mouseYNDC << ")" << std::endl;
    std::cout << "Ray origin: (" << pickEye.x << ", " << pickEye.y << ", " << pickEye.z << ")" << std::endl;
    std::cout << "Ray direction: (" << rayDirection.x << ", " << rayDirection.y << ", " << rayDirection.z << ")" << std::endl;
    
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
    
    std::cout << "  Ray intersection - Origin: (" << ray.origin.x << ", " << ray.origin.y << ", " << ray.origin.z << ")" << std::endl;
    std::cout << "  Ray direction: (" << ray.direction.x << ", " << ray.direction.y << ", " << ray.direction.z << ")" << std::endl;
    
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
            
            std::cout << "  Intersection found at t=" << t << ", point=(" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")" << std::endl;
            return true;
        }
        
        intersectionChecks++;
        t += stepSize;
        
        // Debug first few checks
        if (intersectionChecks <= 3) {
            std::cout << "  Check " << intersectionChecks << ": t=" << t << ", point=(" << currentPoint.x << ", " << currentPoint.y << ", " << currentPoint.z << "), raw_height=" << rawTerrainHeight << ", actual_height=" << actualTerrainHeight << std::endl;
        }
        
        // Early exit if we've gone too far below terrain
        if (intersectionChecks > 50 && currentPoint.y < actualTerrainHeight - 10.0f) {
            break;
        }
    }
    
    std::cout << "  No intersection found after " << intersectionChecks << " checks" << std::endl;
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
    
    uint64_t state = BGFX_STATE_DEFAULT;
    state &= ~BGFX_STATE_CULL_MASK;
    bgfx::setState(state);
    
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
    
    bgfx::VertexBufferHandle npcVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(npcCubeVertices, sizeof(npcCubeVertices)), layout);
    
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
    
    if (!bgfx::isValid(proceduralTexture)) {
        std::cerr << "Failed to create procedural texture!" << std::endl;
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
    std::cout << "Right click - Move player to terrain location" << std::endl;
    std::cout << "Double right click - Sprint to terrain location" << std::endl;
    std::cout << "SPACE - Mine nearby resource nodes" << std::endl;
    std::cout << "I    - Toggle inventory overlay" << std::endl;
    std::cout << "O    - Toggle debug overlay" << std::endl;
    std::cout << "H    - Test damage (health system)" << std::endl;
    std::cout << "J    - Test healing (health system)" << std::endl;
    std::cout << "K    - Test attack nearby NPCs" << std::endl;
    std::cout << "ESC  - Exit" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Main game loop variables
    bool quit = false;
    SDL_Event event;
    float time = 0.0f;
    
    // Camera variables - set to bird's eye view of player
    bx::Vec3 cameraPos = {player.position.x, player.position.y + 15.0f, player.position.z + 8.0f};
    
    // Calculate initial camera orientation to look at player
    float dirX = player.position.x - cameraPos.x;
    float dirY = player.position.y - cameraPos.y;
    float dirZ = player.position.z - cameraPos.z;
    float horizontalDist = bx::sqrt(dirX * dirX + dirZ * dirZ);
    
    float cameraYaw = bx::atan2(dirX, dirZ);
    float cameraPitch = bx::atan2(-dirY, horizontalDist);
    
    std::cout << "Initial camera set to bird's eye view of player" << std::endl;
    
    // Mouse variables for camera rotation
    float prevMouseX = 0.0f;
    float prevMouseY = 0.0f;
    bool mouseDown = false;
    
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
                    cameraPos.x = player.position.x;
                    cameraPos.y = player.position.y + 15.0f; // Bird's eye height
                    cameraPos.z = player.position.z + 8.0f;  // Slightly back from player
                    
                    // Calculate direction vector from camera to player
                    float dirX = player.position.x - cameraPos.x;
                    float dirY = player.position.y - cameraPos.y;
                    float dirZ = player.position.z - cameraPos.z;
                    
                    // Calculate horizontal distance for pitch calculation
                    float horizontalDist = bx::sqrt(dirX * dirX + dirZ * dirZ);
                    
                    // Calculate pitch (looking down at player)
                    cameraPitch = bx::atan2(-dirY, horizontalDist);
                    
                    // Calculate yaw (horizontal rotation toward player)
                    cameraYaw = bx::atan2(dirX, dirZ);
                    
                    std::cout << "Camera jumped to bird's eye view of player (pitch: " << cameraPitch << ", yaw: " << cameraYaw << ")" << std::endl;
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
                            int resourceGained = node.mine();
                            if (resourceGained > 0) {
                                inventory.addResource(node.type, resourceGained);
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
                else if (event.key.key == SDLK_K) {
                    // Test NPC damage (K key for attacking nearby NPCs)
                    const float attackRange = 3.0f;
                    bool attackedSomeone = false;
                    
                    for (auto& npc : npcs) {
                        if (!npc.isActive) continue;
                        
                        float dx = player.position.x - npc.position.x;
                        float dz = player.position.z - npc.position.z;
                        float distance = bx::sqrt(dx * dx + dz * dz);
                        
                        if (distance <= attackRange && npc.canTakeDamage(time)) {
                            npc.takeDamage(25, time);
                            attackedSomeone = true;
                            break; // Attack only one NPC
                        }
                    }
                    
                    if (!attackedSomeone) {
                        std::cout << "K key pressed - No NPCs in attack range (" << attackRange << " units)" << std::endl;
                    }
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
                    mouseDown = true;
                    SDL_GetMouseState(&prevMouseX, &prevMouseY);
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
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouseDown = false;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (mouseDown) {
                    float mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    
                    float deltaX = mouseX - prevMouseX;
                    float deltaY = mouseY - prevMouseY;
                    
                    cameraYaw += deltaX * CAMERA_ROTATE_SPEED;
                    cameraPitch += deltaY * CAMERA_ROTATE_SPEED;
                    
                    if (cameraPitch > bx::kPi * 0.49f) cameraPitch = bx::kPi * 0.49f;
                    if (cameraPitch < -bx::kPi * 0.49f) cameraPitch = -bx::kPi * 0.49f;
                    
                    prevMouseX = mouseX;
                    prevMouseY = mouseY;
                }
            }
        }
        
        keyboardState = SDL_GetKeyboardState(NULL);
        
        // Process WASD movement
        float forward[3] = { 
            bx::sin(cameraYaw) * bx::cos(cameraPitch),
            -bx::sin(cameraPitch),
            bx::cos(cameraYaw) * bx::cos(cameraPitch)
        };
        
        float right[3] = {
            bx::cos(cameraYaw), 0.0f, -bx::sin(cameraYaw)
        };
        
        bool sprinting = keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT];
        float currentSpeed = CAMERA_MOVE_SPEED * (sprinting ? CAMERA_SPRINT_MULTIPLIER : 1.0f);
        
        if (keyboardState[SDL_SCANCODE_W]) {
            cameraPos.x += forward[0] * currentSpeed;
            cameraPos.y += forward[1] * currentSpeed;
            cameraPos.z += forward[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_S]) {
            cameraPos.x -= forward[0] * currentSpeed;
            cameraPos.y -= forward[1] * currentSpeed;
            cameraPos.z -= forward[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_A]) {
            cameraPos.x -= right[0] * currentSpeed;
            cameraPos.y -= right[1] * currentSpeed;
            cameraPos.z -= right[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_D]) {
            cameraPos.x += right[0] * currentSpeed;
            cameraPos.y += right[1] * currentSpeed;
            cameraPos.z += right[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_Q]) {
            cameraPos.y += currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_E]) {
            cameraPos.y -= currentSpeed;
        }
        
        time += 0.01f;
        float deltaTime = 0.01f; // Fixed delta time for now

        // Set up camera view matrix
        bx::Vec3 target = {
            cameraPos.x + forward[0],
            cameraPos.y + forward[1],
            cameraPos.z + forward[2]
        };
        
        float view[16];
        bx::mtxLookAt(view, cameraPos, target);
        
        float proj[16];
        bx::mtxProj(proj, 60.0f, float(WINDOW_WIDTH) / float(WINDOW_HEIGHT), 0.1f, 100.0f, 
                   bgfx::getCaps()->homogeneousDepth);
        
        bgfx::setViewTransform(0, view, proj);
        bgfx::setViewRect(0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        // Handle terrain picking from right-click
        if (hasPendingClick) {
            int currentWidth, currentHeight;
            SDL_GetWindowSize(window, &currentWidth, &currentHeight);
            
            std::cout << "Right-click detected! Window size: " << currentWidth << "x" << currentHeight << std::endl;
            Ray ray = createRayFromMouse(pendingMouseX, pendingMouseY, currentWidth, currentHeight, view, proj);
            bx::Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
            if (rayTerrainIntersection(ray, chunkManager, hitPoint)) {
                player.setTarget(hitPoint.x, hitPoint.z, chunkManager, shouldSprint);
                if (shouldSprint) {
                    std::cout << "Player sprinting to: (" << hitPoint.x << ", " << hitPoint.z << ")" << std::endl;
                } else {
                    std::cout << "Player moving to: (" << hitPoint.x << ", " << hitPoint.z << ")" << std::endl;
                }
            } else {
                std::cout << "No terrain intersection found!" << std::endl;
            }
            hasPendingClick = false;
            shouldSprint = false; // Reset sprint flag
        }
        
        // Update debug overlay
        debugOverlay.update();
        
        // Update player and chunks
        player.update(chunkManager);
        chunkManager.updateChunksAroundPlayer(player.position.x, player.position.z);
        
        // Render all loaded terrain chunks
        chunkManager.renderChunks(texProgram, s_texColor);
        
        // Render player as a colored cube
        float playerMatrix[16], playerTranslation[16], playerScale[16];
        bx::mtxScale(playerScale, player.size, player.size, player.size);
        bx::mtxTranslate(playerTranslation, player.position.x, player.position.y, player.position.z);
        bx::mtxMul(playerMatrix, playerScale, playerTranslation);
        render_object_at_position(vbh, ibh, program, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, playerMatrix);
        
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
            npc.update(deltaTime, npcTerrainHeight);
            
            // Render NPC
            float npcMatrix[16], npcTranslation[16], npcScale[16];
            bx::mtxScale(npcScale, npc.size, npc.size, npc.size);
            bx::mtxTranslate(npcTranslation, npc.position.x, npc.position.y, npc.position.z);
            bx::mtxMul(npcMatrix, npcScale, npcTranslation);
            
            // Use NPC vertex buffer (we'll update colors dynamically later if needed)
            render_object_at_position(npcVbh, ibh, program, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, npcMatrix);
        }
        
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
        render_object_at_position(texVbh, texIbh, texProgram, proceduralTexture, s_texColor, texturedMtx);
        
        // Render PNG textured cube (top) if available
        if (bgfx::isValid(pngTexture)) {
            float pngTexMtx[16];
            bx::mtxTranslate(translation, 0.0f, 2.5f, 0.0f);
            bx::mtxRotateXY(rotation, time * 0.15f, time * 0.3f);
            bx::mtxMul(pngTexMtx, rotation, translation);
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
            
            gardenLampModel.render(texProgram, s_texColor, modelMatrix);
        }
        
        // Always clear debug text buffer and render active overlays
        bgfx::dbgTextClear();
        
        // Get current window size for positioning
        int currentWidth, currentHeight;
        SDL_GetWindowSize(window, &currentWidth, &currentHeight);
        
        // Calculate text grid size (BGFX debug text uses character grid)
        int textCols = currentWidth / 8;  // Approximate character width
        int textRows = currentHeight / 16; // Approximate character height
        
        // Render debug overlay if enabled
        if (debugOverlay.enabled) {
            // Position FPS counter in top right corner
            int fpsX = textCols - 18; // Leave more space for "FPS: 999 (99.9ms)"
            if (fpsX < 0) fpsX = 0;
            
            // Render FPS counter with top padding
            bgfx::dbgTextPrintf(fpsX, 1, 0x0f, "FPS: %3d (%4.1fms)", (int)debugOverlay.fps, debugOverlay.frameTime);
            
            // Optional: Add more debug info below FPS
            bgfx::dbgTextPrintf(fpsX, 2, 0x0f, "Chunks: %d", (int)chunkManager.getLoadedChunkInfo().size());
            bgfx::dbgTextPrintf(fpsX, 3, 0x0f, "Player: %.1f,%.1f", player.position.x, player.position.z);
        }
        
        // Render inventory overlay if enabled
        inventory.renderOverlay();
        
        // Render player health bar (always visible)
        player.renderHealthBar();
        
        bgfx::frame();
    }
    
    // Clean up resources
    bgfx::destroy(proceduralTexture);
    if (bgfx::isValid(pngTexture)) bgfx::destroy(pngTexture);
    bgfx::destroy(s_texColor);
    bgfx::destroy(ibh);
    bgfx::destroy(vbh);
    bgfx::destroy(copperVbh);
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