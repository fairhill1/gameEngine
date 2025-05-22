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

// Terrain system
struct TerrainVertex {
    float x, y, z;
    float u, v;
};

class Terrain {
public:
    static constexpr int SIZE = 64;
    static constexpr float SCALE = 0.5f;
    static constexpr float HEIGHT_SCALE = 3.0f;
    
    std::vector<TerrainVertex> vertices;
    std::vector<uint16_t> indices;
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
    
    void generate() {
        vertices.clear();
        indices.clear();
        
        // Generate vertices with smooth multi-octave noise
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for consistent terrain
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        // Pre-generate smooth noise values using interpolated random values
        std::vector<std::vector<float>> noiseMap(SIZE, std::vector<float>(SIZE));
        
        // Generate base random values at lower resolution then interpolate
        const int noiseRes = 8; // Lower resolution for base noise
        std::vector<std::vector<float>> baseNoise(noiseRes + 1, std::vector<float>(noiseRes + 1));
        
        for (int z = 0; z <= noiseRes; z++) {
            for (int x = 0; x <= noiseRes; x++) {
                baseNoise[x][z] = dis(gen) - 0.5f;
            }
        }
        
        // Interpolate noise values to full resolution
        for (int z = 0; z < SIZE; z++) {
            for (int x = 0; x < SIZE; x++) {
                float fx = float(x) / float(SIZE - 1) * noiseRes;
                float fz = float(z) / float(SIZE - 1) * noiseRes;
                
                int x1 = int(fx);
                int z1 = int(fz);
                int x2 = x1 + 1;
                int z2 = z1 + 1;
                
                if (x2 > noiseRes) x2 = noiseRes;
                if (z2 > noiseRes) z2 = noiseRes;
                
                float fracX = fx - x1;
                float fracZ = fz - z1;
                
                // Bilinear interpolation for smooth transitions
                float n1 = baseNoise[x1][z1] * (1 - fracX) + baseNoise[x2][z1] * fracX;
                float n2 = baseNoise[x1][z2] * (1 - fracX) + baseNoise[x2][z2] * fracX;
                noiseMap[x][z] = n1 * (1 - fracZ) + n2 * fracZ;
            }
        }
        
        for (int z = 0; z < SIZE; z++) {
            for (int x = 0; x < SIZE; x++) {
                float worldX = (x - SIZE/2) * SCALE;
                float worldZ = (z - SIZE/2) * SCALE;
                
                // Multi-octave height generation for natural terrain
                float height = 0.0f;
                
                // Large scale rolling hills (octave 1)
                height += noiseMap[x][z] * HEIGHT_SCALE * 0.8f;
                
                // Medium scale features (octave 2)
                float mediumNoise = bx::sin(worldX * 0.05f) * bx::cos(worldZ * 0.05f);
                height += mediumNoise * HEIGHT_SCALE * 0.3f;
                
                // Fine detail (octave 3)
                float fineNoise = bx::sin(worldX * 0.2f + worldZ * 0.1f) * bx::cos(worldZ * 0.2f);
                height += fineNoise * HEIGHT_SCALE * 0.1f;
                
                // Add subtle distance-based falloff for more natural edges
                float distanceFromCenter = bx::sqrt(worldX * worldX + worldZ * worldZ);
                float falloff = bx::clamp(1.0f - distanceFromCenter / (SIZE * SCALE * 0.4f), 0.0f, 1.0f);
                height *= falloff * falloff; // Smooth edge falloff
                
                TerrainVertex vertex;
                vertex.x = worldX;
                vertex.y = height;
                vertex.z = worldZ;
                vertex.u = float(x) / (SIZE - 1);
                vertex.v = float(z) / (SIZE - 1);
                
                vertices.push_back(vertex);
            }
        }
        
        // Generate indices for triangles
        for (int z = 0; z < SIZE - 1; z++) {
            for (int x = 0; x < SIZE - 1; x++) {
                int topLeft = z * SIZE + x;
                int topRight = z * SIZE + (x + 1);
                int bottomLeft = (z + 1) * SIZE + x;
                int bottomRight = (z + 1) * SIZE + (x + 1);
                
                // First triangle
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
                
                // Second triangle
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }
        
        std::cout << "Generated terrain with " << vertices.size() << " vertices and " 
                  << indices.size() << " indices" << std::endl;
    }
    
    void createBuffers() {
        const bgfx::Memory* vertexMem = bgfx::copy(vertices.data(), vertices.size() * sizeof(TerrainVertex));
        const bgfx::Memory* indexMem = bgfx::copy(indices.data(), indices.size() * sizeof(uint16_t));
        
        bgfx::VertexLayout terrainLayout;
        terrainLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        
        vbh = bgfx::createVertexBuffer(vertexMem, terrainLayout);
        ibh = bgfx::createIndexBuffer(indexMem);
    }
    
    float getHeightAt(float x, float z) const {
        // Convert world coordinates to terrain grid coordinates
        float gridX = (x / SCALE) + SIZE/2;
        float gridZ = (z / SCALE) + SIZE/2;
        
        // Clamp to terrain bounds
        if (gridX < 0 || gridX >= SIZE-1 || gridZ < 0 || gridZ >= SIZE-1) {
            return 0.0f;
        }
        
        int x1 = (int)gridX;
        int z1 = (int)gridZ;
        int x2 = x1 + 1;
        int z2 = z1 + 1;
        
        float fx = gridX - x1;
        float fz = gridZ - z1;
        
        // Get heights at four corners
        float h1 = vertices[z1 * SIZE + x1].y;
        float h2 = vertices[z1 * SIZE + x2].y;
        float h3 = vertices[z2 * SIZE + x1].y;
        float h4 = vertices[z2 * SIZE + x2].y;
        
        // Bilinear interpolation
        float top = h1 * (1 - fx) + h2 * fx;
        float bottom = h3 * (1 - fx) + h4 * fx;
        return top * (1 - fz) + bottom * fz;
    }
};

// Player system
struct Player {
    bx::Vec3 position;
    bx::Vec3 targetPosition;
    bool hasTarget;
    float moveSpeed;
    float size;
    
    Player() : position({0.0f, 0.0f, 0.0f}), targetPosition({0.0f, 0.0f, 0.0f}), 
               hasTarget(false), moveSpeed(0.05f), size(0.3f) {}
    
    void setTarget(float x, float z, const Terrain& terrain) {
        targetPosition.x = x;
        targetPosition.z = z;
        targetPosition.y = terrain.getHeightAt(x, z) + size - 5.0f; // Account for terrain offset
        hasTarget = true;
    }
    
    void update(const Terrain& terrain) {
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
            } else {
                direction.x /= distance;
                direction.z /= distance;
                
                position.x += direction.x * moveSpeed;
                position.z += direction.z * moveSpeed;
                position.y = terrain.getHeightAt(position.x, position.z) + size - 5.0f; // Account for terrain offset
            }
        }
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

bool rayTerrainIntersection(const Ray& ray, const Terrain& terrain, bx::Vec3& hitPoint) {
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
        
        float rawTerrainHeight = terrain.getHeightAt(currentPoint.x, currentPoint.z);
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

// Create procedural texture
bgfx::TextureHandle create_procedural_texture() {
    std::cout << "Creating procedural texture..." << std::endl;
    
    const uint32_t textureWidth = 256;
    const uint32_t textureHeight = 256;
    const uint32_t textureSize = textureWidth * textureHeight * 4;
    
    const bgfx::Memory* texMem = bgfx::alloc(textureSize);
    uint8_t* data = texMem->data;
    
    // Create grain patterns
    std::vector<std::vector<float>> grainPattern(textureWidth, std::vector<float>(textureHeight, 0.0f));
    
    for (int i = 0; i < 200; ++i) {
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
    
    // Generate texture pixels
    for (uint32_t y = 0; y < textureHeight; ++y) {
        for (uint32_t x = 0; x < textureWidth; ++x) {
            uint32_t offset = (y * textureWidth + x) * 4;
            
            uint8_t r = 220, g = 185, b = 140;
            uint8_t microNoise = (uint8_t)((rand() % 20) - 10);
            
            float grainValue = bx::clamp(grainPattern[x][y], 0.0f, 1.0f);
            float darkening = 1.0f - (grainValue * 0.3f);
            
            bool isDarkerBand = ((y / 12) % 2) == 0;
            float bandFactor = isDarkerBand ? 0.9f : 1.0f;
            
            r = (uint8_t)bx::clamp((r * darkening * bandFactor) + microNoise, 160.0f, 240.0f);
            g = (uint8_t)bx::clamp((g * darkening * bandFactor) + microNoise, 130.0f, 210.0f);
            b = (uint8_t)bx::clamp((b * darkening * bandFactor) + microNoise, 100.0f, 170.0f);
            
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
    
    // Create terrain and player
    std::cout << "Creating terrain..." << std::endl;
    Terrain terrain;
    terrain.generate();
    terrain.createBuffers();
    
    std::cout << "Creating player..." << std::endl;
    Player player;
    player.position = {0.0f, terrain.getHeightAt(0.0f, 0.0f) + player.size - 5.0f, 0.0f};
    
    std::cout << "Starting main loop..." << std::endl;
    std::cout << "===== Controls =====" << std::endl;
    std::cout << "WASD - Move camera" << std::endl;
    std::cout << "SHIFT - Sprint (faster movement)" << std::endl;
    std::cout << "Q/E  - Move up/down" << std::endl;
    std::cout << "Left click and drag - Rotate camera" << std::endl;
    std::cout << "Right click - Move player to terrain location" << std::endl;
    std::cout << "ESC  - Exit" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Main game loop variables
    bool quit = false;
    SDL_Event event;
    float time = 0.0f;
    
    // Camera variables
    bx::Vec3 cameraPos = {0.0f, 8.0f, -10.0f};
    float cameraYaw = 0.0f;
    float cameraPitch = -0.3f;
    
    // Mouse variables for camera rotation
    float prevMouseX = 0.0f;
    float prevMouseY = 0.0f;
    bool mouseDown = false;
    
    // Mouse variables for terrain picking
    float pendingMouseX = 0.0f;
    float pendingMouseY = 0.0f;
    bool hasPendingClick = false;
    
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
                    // Right click for terrain picking
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
            if (rayTerrainIntersection(ray, terrain, hitPoint)) {
                player.setTarget(hitPoint.x, hitPoint.z, terrain);
                std::cout << "Player moving to: (" << hitPoint.x << ", " << hitPoint.z << ")" << std::endl;
            } else {
                std::cout << "No terrain intersection found!" << std::endl;
            }
            hasPendingClick = false;
        }
        
        // Update player
        player.update(terrain);
        
        // Render terrain (moved down to avoid intersecting with cubes)
        float terrainMatrix[16], terrainTranslation[16];
        bx::mtxTranslate(terrainTranslation, 0.0f, -5.0f, 0.0f);
        bx::mtxIdentity(terrainMatrix);
        bx::mtxMul(terrainMatrix, terrainMatrix, terrainTranslation);
        render_object_at_position(terrain.vbh, terrain.ibh, texProgram, proceduralTexture, s_texColor, terrainMatrix);
        
        // Render player as a colored cube
        float playerMatrix[16], playerTranslation[16], playerScale[16];
        bx::mtxScale(playerScale, player.size, player.size, player.size);
        bx::mtxTranslate(playerTranslation, player.position.x, player.position.y, player.position.z);
        bx::mtxMul(playerMatrix, playerScale, playerTranslation);
        render_object_at_position(vbh, ibh, program, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, playerMatrix);
        
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
        
        bgfx::frame();
    }
    
    // Clean up resources
    bgfx::destroy(proceduralTexture);
    if (bgfx::isValid(pngTexture)) bgfx::destroy(pngTexture);
    bgfx::destroy(s_texColor);
    bgfx::destroy(ibh);
    bgfx::destroy(vbh);
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