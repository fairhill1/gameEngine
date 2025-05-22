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

// Structure for a glTF model mesh
struct ModelMesh {
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
    int numIndices;
    
    // Texture/material information
    bgfx::TextureHandle diffuseTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle normalTexture = BGFX_INVALID_HANDLE;
};

// Class to handle loading and rendering a glTF/GLB model
class Model {
public:
    Model() = default;
    ~Model() {
        unload();
    }
    
    bool loadFromFile(const char* filepath) {
        std::cout << "Loading model from: " << filepath << std::endl;
        
        // Setup tinygltf loader
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        
        // Set up custom image loader (we disabled the built-in STB loader)
        loader.SetImageLoader(
            [](tinygltf::Image* image, const int imageIndex, 
               std::string* err, std::string* warn, int req_width, int req_height, 
               const unsigned char* bytes, int size, void* userData) -> bool {
                
                // If we have external image data
                if (bytes && size > 0) {
                    // Load image using stb_image 
                    int width, height, channels;
                    unsigned char* data = stbi_load_from_memory(bytes, size, &width, &height, &channels, STBI_default);
                    
                    if (!data) {
                        if (err) {
                            *err = std::string("Failed to load image from memory: ") + stbi_failure_reason();
                        }
                        return false;
                    }
                    
                    // Store the image data
                    image->width = width;
                    image->height = height;
                    image->component = channels;
                    image->image.resize(static_cast<size_t>(width * height * channels));
                    std::memcpy(image->image.data(), data, width * height * channels);
                    
                    // Free the stb data
                    stbi_image_free(data);
                    
                    return true;
                }
                
                // If we have a URI pointing to an image file
                if (!image->uri.empty()) {
                    // Load image using stb_image
                    int width, height, channels;
                    unsigned char* data = stbi_load(image->uri.c_str(), &width, &height, &channels, STBI_default);
                    
                    if (!data) {
                        if (err) {
                            *err = std::string("Failed to load image from file: ") + stbi_failure_reason();
                        }
                        return false;
                    }
                    
                    // Store the image data
                    image->width = width;
                    image->height = height;
                    image->component = channels;
                    image->image.resize(static_cast<size_t>(width * height * channels));
                    std::memcpy(image->image.data(), data, width * height * channels);
                    
                    // Free the stb data
                    stbi_image_free(data);
                    
                    return true;
                }
                
                if (err) {
                    *err = "No image data provided";
                }
                return false;
            },
            nullptr // User data
        );
        
        // Determine if it's a binary GLB or text-based glTF by file extension
        bool result = false;
        std::string filepathStr(filepath);
        if (filepathStr.size() > 4 && filepathStr.substr(filepathStr.size() - 4) == ".glb") {
            result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
        } else {
            result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
        }
        
        // Check for errors
        if (!warn.empty()) {
            std::cout << "glTF warning: " << warn << std::endl;
        }
        
        if (!err.empty()) {
            std::cerr << "glTF error: " << err << std::endl;
            return false;
        }
        
        if (!result) {
            std::cerr << "Failed to load glTF model." << std::endl;
            return false;
        }
        
        // Process the model
        return processModel(gltfModel);
    }
    
    void render(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform, float* modelMatrix) {
        // Set model matrix for the entire model
        bgfx::setTransform(modelMatrix);
        
        // Render each mesh
        for (const auto& mesh : meshes) {
            // Set textures if available
            if (bgfx::isValid(mesh.diffuseTexture)) {
                bgfx::setTexture(0, texUniform, mesh.diffuseTexture);
            }
            
            // Set vertex and index buffers
            bgfx::setVertexBuffer(0, mesh.vbh);
            bgfx::setIndexBuffer(mesh.ibh);
            
            // Set render states - disable backface culling to ensure all faces are visible
            uint64_t state = BGFX_STATE_DEFAULT;
            state &= ~BGFX_STATE_CULL_MASK;   // Remove any culling flags
            bgfx::setState(state);
            
            // Submit for rendering
            bgfx::submit(0, program);
        }
    }
    
    void unload() {
        // Free all resources
        for (const auto& mesh : meshes) {
            if (bgfx::isValid(mesh.vbh)) {
                bgfx::destroy(mesh.vbh);
            }
            if (bgfx::isValid(mesh.ibh)) {
                bgfx::destroy(mesh.ibh);
            }
            if (bgfx::isValid(mesh.diffuseTexture)) {
                bgfx::destroy(mesh.diffuseTexture);
            }
            if (bgfx::isValid(mesh.normalTexture)) {
                bgfx::destroy(mesh.normalTexture);
            }
        }
        meshes.clear();
    }
    
public:
    std::vector<ModelMesh> meshes;
    
private:
    bool processModel(const tinygltf::Model& gltfModel) {
        // Process all meshes in the model
        for (const auto& mesh : gltfModel.meshes) {
            std::cout << "Processing mesh: " << mesh.name << std::endl;
            
            // Process each primitive (submesh) in the mesh
            for (const auto& primitive : mesh.primitives) {
                // Create a ModelMesh structure to hold the data
                ModelMesh modelMesh;
                
                // Get accessor indices for the primitive attributes
                auto positionAccessorIt = primitive.attributes.find("POSITION");
                auto normalAccessorIt = primitive.attributes.find("NORMAL");
                auto texcoordAccessorIt = primitive.attributes.find("TEXCOORD_0");
                
                // Check if we have the minimum required attributes (position)
                if (positionAccessorIt == primitive.attributes.end()) {
                    std::cerr << "Mesh is missing POSITION attribute, skipping" << std::endl;
                    continue;
                }
                
                // Get accessor indices
                int positionAccessorIndex = positionAccessorIt->second;
                int normalAccessorIndex = normalAccessorIt != primitive.attributes.end() ? normalAccessorIt->second : -1;
                int texcoordAccessorIndex = texcoordAccessorIt != primitive.attributes.end() ? texcoordAccessorIt->second : -1;
                
                // Process vertex and index data
                if (!processVertices(gltfModel, primitive, positionAccessorIndex, normalAccessorIndex, texcoordAccessorIndex, modelMesh)) {
                    std::cerr << "Failed to process vertices, skipping mesh" << std::endl;
                    continue;
                }
                
                // Process material if available
                if (primitive.material >= 0) {
                    processMaterial(gltfModel, primitive.material, modelMesh);
                }
                
                // Add the mesh to our collection
                meshes.push_back(modelMesh);
            }
        }
        
        std::cout << "Loaded " << meshes.size() << " meshes from model" << std::endl;
        return !meshes.empty();
    }
    
    // Vertex structure to match our shader input
    struct ModelVertex {
        float x, y, z;       // Position
        float nx, ny, nz;    // Normal (optional)
        float u, v;          // Texture coordinates (optional)
    };
    
    bool processVertices(const tinygltf::Model& gltfModel, const tinygltf::Primitive& primitive, 
                        int positionAccessorIndex, int normalAccessorIndex, int texcoordAccessorIndex,
                        ModelMesh& modelMesh) {
        
        // Get the accessors for positions, normals, and texture coordinates
        const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionAccessorIndex];
        const tinygltf::BufferView& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
        const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionBufferView.buffer];
        
        // Optional normal data
        const tinygltf::Accessor* normalAccessor = nullptr;
        const tinygltf::BufferView* normalBufferView = nullptr;
        const tinygltf::Buffer* normalBuffer = nullptr;
        
        if (normalAccessorIndex >= 0) {
            normalAccessor = &gltfModel.accessors[normalAccessorIndex];
            normalBufferView = &gltfModel.bufferViews[normalAccessor->bufferView];
            normalBuffer = &gltfModel.buffers[normalBufferView->buffer];
        }
        
        // Optional texture coordinate data
        const tinygltf::Accessor* texcoordAccessor = nullptr;
        const tinygltf::BufferView* texcoordBufferView = nullptr;
        const tinygltf::Buffer* texcoordBuffer = nullptr;
        
        if (texcoordAccessorIndex >= 0) {
            texcoordAccessor = &gltfModel.accessors[texcoordAccessorIndex];
            texcoordBufferView = &gltfModel.bufferViews[texcoordAccessor->bufferView];
            texcoordBuffer = &gltfModel.buffers[texcoordBufferView->buffer];
        }
        
        // Get number of vertices
        size_t vertexCount = positionAccessor.count;
        std::cout << "Vertex count: " << vertexCount << std::endl;
        
        // Prepare vertex data array
        std::vector<ModelVertex> vertices(vertexCount);
        
        // Extract position data
        for (size_t i = 0; i < vertexCount; i++) {
            // Calculate offset into the buffer
            size_t posOffset = positionBufferView.byteOffset + positionAccessor.byteOffset + i * 12; // 3 floats * 4 bytes
            const float* position = reinterpret_cast<const float*>(&positionBuffer.data[posOffset]);
            
            // Set vertex position
            vertices[i].x = position[0];
            vertices[i].y = position[1];
            vertices[i].z = position[2];
            
            // Default normal
            vertices[i].nx = 0.0f;
            vertices[i].ny = 1.0f;
            vertices[i].nz = 0.0f;
            
            // Default texture coordinates
            vertices[i].u = 0.0f;
            vertices[i].v = 0.0f;
        }
        
        // Extract normal data if available
        if (normalAccessor) {
            for (size_t i = 0; i < vertexCount; i++) {
                size_t normOffset = normalBufferView->byteOffset + normalAccessor->byteOffset + i * 12; // 3 floats * 4 bytes
                const float* normal = reinterpret_cast<const float*>(&normalBuffer->data[normOffset]);
                
                vertices[i].nx = normal[0];
                vertices[i].ny = normal[1];
                vertices[i].nz = normal[2];
            }
        }
        
        // Extract texture coordinates if available
        if (texcoordAccessor) {
            for (size_t i = 0; i < vertexCount; i++) {
                size_t texOffset = texcoordBufferView->byteOffset + texcoordAccessor->byteOffset + i * 8; // 2 floats * 4 bytes
                const float* texcoord = reinterpret_cast<const float*>(&texcoordBuffer->data[texOffset]);
                
                vertices[i].u = texcoord[0];
                vertices[i].v = texcoord[1];
            }
        }
        
        // Create vertex buffer
        bgfx::VertexLayout vertexLayout;
        vertexLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        
        modelMesh.vbh = bgfx::createVertexBuffer(
            bgfx::makeRef(vertices.data(), vertices.size() * sizeof(ModelVertex)),
            vertexLayout
        );
        
        // Process indices
        if (primitive.indices >= 0) {
            const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
            const tinygltf::BufferView& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
            
            modelMesh.numIndices = indexAccessor.count;
            std::cout << "Index count: " << modelMesh.numIndices << std::endl;
            
            // Convert indices to uint16_t (regardless of the source format)
            std::vector<uint16_t> indices(modelMesh.numIndices);
            
            // Get component type (5123 = UNSIGNED_SHORT, 5125 = UNSIGNED_INT, etc.)
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                // Already uint16_t, just copy
                for (size_t i = 0; i < modelMesh.numIndices; i++) {
                    size_t offset = indexBufferView.byteOffset + indexAccessor.byteOffset + i * 2;
                    indices[i] = *reinterpret_cast<const uint16_t*>(&indexBuffer.data[offset]);
                }
            } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                // Convert uint32_t to uint16_t (with potential loss for large models)
                for (size_t i = 0; i < modelMesh.numIndices; i++) {
                    size_t offset = indexBufferView.byteOffset + indexAccessor.byteOffset + i * 4;
                    uint32_t index = *reinterpret_cast<const uint32_t*>(&indexBuffer.data[offset]);
                    indices[i] = static_cast<uint16_t>(index); // Note: This may truncate for large indices
                }
            } else {
                std::cerr << "Unsupported index component type: " << indexAccessor.componentType << std::endl;
                return false;
            }
            
            modelMesh.ibh = bgfx::createIndexBuffer(
                bgfx::makeRef(indices.data(), indices.size() * sizeof(uint16_t))
            );
        } else {
            // No indices, use triangle list
            std::vector<uint16_t> indices(vertexCount);
            for (uint16_t i = 0; i < vertexCount; i++) {
                indices[i] = i;
            }
            
            modelMesh.numIndices = indices.size();
            modelMesh.ibh = bgfx::createIndexBuffer(
                bgfx::makeRef(indices.data(), indices.size() * sizeof(uint16_t))
            );
        }
        
        return true;
    }
    
    void processMaterial(const tinygltf::Model& gltfModel, int materialIndex, ModelMesh& modelMesh) {
        const tinygltf::Material& material = gltfModel.materials[materialIndex];
        
        // Process textures if available
        // Base color texture
        if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            int sourceIndex = gltfModel.textures[textureIndex].source;
            
            if (sourceIndex >= 0) {
                modelMesh.diffuseTexture = loadTexture(gltfModel, sourceIndex);
            }
        }
        
        // Normal map texture
        if (material.normalTexture.index >= 0) {
            int textureIndex = material.normalTexture.index;
            int sourceIndex = gltfModel.textures[textureIndex].source;
            
            if (sourceIndex >= 0) {
                modelMesh.normalTexture = loadTexture(gltfModel, sourceIndex);
            }
        }
    }
    
    bgfx::TextureHandle loadTexture(const tinygltf::Model& gltfModel, int imageIndex) {
        const tinygltf::Image& image = gltfModel.images[imageIndex];
        
        // Check if we have valid image data
        if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
            return BGFX_INVALID_HANDLE;
        }
        
        // Determine the format
        bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;
        int componentsPerPixel = image.component;
        
        if (componentsPerPixel == 3) {
            // RGB image - need to convert to RGBA for BGFX
            std::vector<uint8_t> rgbaData(image.width * image.height * 4);
            
            for (int i = 0; i < image.width * image.height; i++) {
                rgbaData[i * 4 + 0] = image.image[i * 3 + 0]; // R
                rgbaData[i * 4 + 1] = image.image[i * 3 + 1]; // G
                rgbaData[i * 4 + 2] = image.image[i * 3 + 2]; // B
                rgbaData[i * 4 + 3] = 255;                     // A (fully opaque)
            }
            
            // Create the texture
            const bgfx::Memory* mem = bgfx::copy(rgbaData.data(), rgbaData.size());
            
            bgfx::TextureHandle texture = bgfx::createTexture2D(
                image.width, 
                image.height,
                false, 
                1,
                format,
                BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC,
                mem
            );
            
            return texture;
        } else if (componentsPerPixel == 4) {
            // RGBA image - use directly
            const bgfx::Memory* mem = bgfx::copy(image.image.data(), image.image.size());
            
            bgfx::TextureHandle texture = bgfx::createTexture2D(
                image.width, 
                image.height,
                false, 
                1,
                format,
                BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC,
                mem
            );
            
            return texture;
        }
        
        // Unsupported format
        return BGFX_INVALID_HANDLE;
    }
};

// Window dimensions
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const char* WINDOW_TITLE = "My First C++ Game with BGFX";

// Colors
const uint32_t CLEAR_COLOR = 0x303030ff;

// Camera settings
const float CAMERA_MOVE_SPEED = 0.1f;     // Normal movement speed
const float CAMERA_SPRINT_MULTIPLIER = 3.0f; // Speed multiplier when Shift is pressed
const float CAMERA_ROTATE_SPEED = 0.005f; // Mouse rotation speed

// Vertex structures
struct PosColorVertex {
    float x;
    float y;
    float z;
    uint32_t abgr;
};

struct PosTexVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
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
    // Set default texture flags with nice filtering
    uint64_t textureFlags = BGFX_TEXTURE_NONE;
    textureFlags |= BGFX_SAMPLER_MIN_ANISOTROPIC;
    textureFlags |= BGFX_SAMPLER_MAG_ANISOTROPIC;
    
    int width, height, channels;
    
    // Load the image with stb_image, flipping vertically to match OpenGL/DirectX/Metal conventions
    stbi_set_flip_vertically_on_load(true);
    
    // Load the image
    unsigned char* imageData = stbi_load(filePath, &width, &height, &channels, STBI_rgb_alpha);
    
    if (!imageData) {
        std::cerr << "Failed to load texture: " << filePath << std::endl;
        std::cerr << "stb_image error: " << stbi_failure_reason() << std::endl;
        return BGFX_INVALID_HANDLE; // Return invalid handle on failure
    }
    
    std::cout << "Loaded texture: " << filePath << " (" << width << "x" << height << ", " 
              << channels << " channels)" << std::endl;
    
    // Calculate the size of the image data (always use RGBA format = 4 bytes per pixel)
    uint32_t size = width * height * 4;
    
    // Create memory block for BGFX - it will copy this data
    const bgfx::Memory* mem = bgfx::copy(imageData, size);
    
    // Free the original image data as BGFX has copied it
    stbi_image_free(imageData);
    
    // Create BGFX texture object from the memory
    bgfx::TextureHandle handle = bgfx::createTexture2D(
        width, height, 
        false, // No mipmaps for simplicity
        1,     // Layers
        bgfx::TextureFormat::RGBA8, // Format that matches our pixel data
        textureFlags,
        mem    // Memory containing the image data
    );
    
    if (!bgfx::isValid(handle)) {
        std::cerr << "Failed to create BGFX texture from PNG data" << std::endl;
    }
    
    return handle;
}

// Helper function to get SDL window native data
bool get_native_window_info(SDL_Window* window, void** native_window, void** native_display) {
#if BX_PLATFORM_OSX
    // For macOS with SDL3, we need to use the property system
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    *native_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    *native_display = nullptr;
    return *native_window != nullptr;
#elif BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    // For Linux/BSD with SDL3
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    *native_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_WINDOW_POINTER, NULL);
    *native_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    return *native_window != nullptr;
#elif BX_PLATFORM_WINDOWS
    // For Windows with SDL3
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    *native_window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    *native_display = nullptr;
    return *native_window != nullptr;
#else
    return false;
#endif
}

int main(int argc, char* argv[]) {
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Declare variables for our third cube (PNG textured)
    bgfx::VertexBufferHandle png_texVbh;
    bgfx::IndexBufferHandle png_texIbh;
    bgfx::TextureHandle png_texture = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle png_texProgram;
    bgfx::UniformHandle png_s_texColor;
    
    // Create a model object for the Spartan GLB model
    Model spartanModel;

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

    // Initialize BGFX - following the approach from the sample code
    std::cout << "Initializing BGFX..." << std::endl;
    
    // First render a frame to create an implicit context
    bgfx::renderFrame();
    
    // Then initialize BGFX
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
        bgfx::makeRef(cubeVertices, sizeof(cubeVertices)),
        layout
    );
    
    bgfx::VertexBufferHandle texVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(texCubeVertices, sizeof(texCubeVertices)),
        texLayout
    );

    // Create index buffers
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(cubeIndices, sizeof(cubeIndices))
    );
    
    bgfx::IndexBufferHandle texIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(texCubeIndices, sizeof(texCubeIndices))
    );
    
    // Create a procedural texture as a fallback since we're having issues with the PNG loader
    std::cout << "Creating procedural texture..." << std::endl;
    
    // Create a 256x256 texture
    const uint32_t textureWidth = 256;
    const uint32_t textureHeight = 256;
    const uint32_t textureSize = textureWidth * textureHeight * 4; // RGBA
    
    // Allocate memory for the texture
    const bgfx::Memory* texMem = bgfx::alloc(textureSize);
    
    // Fill with a realistic sandy texture pattern
    uint8_t* data = texMem->data;
    
    // Create some larger grain patterns first
    std::vector<std::vector<float>> grainPattern(textureWidth, std::vector<float>(textureHeight, 0.0f));
    
    // Generate a noise pattern for larger grains
    for (int i = 0; i < 200; ++i) {
        int grainX = rand() % textureWidth;
        int grainY = rand() % textureHeight;
        int grainSize = 3 + (rand() % 4);  // Random size between 3-6 pixels
        float grainIntensity = 0.7f + (float)(rand() % 30) / 100.0f;  // Random intensity
        
        // Create a circular grain
        for (int y = -grainSize; y <= grainSize; ++y) {
            for (int x = -grainSize; x <= grainSize; ++x) {
                int posX = grainX + x;
                int posY = grainY + y;
                
                // Check if in bounds
                if (posX >= 0 && posX < textureWidth && posY >= 0 && posY < textureHeight) {
                    // Distance from center of grain
                    float dist = std::sqrt(x*x + y*y);
                    if (dist <= grainSize) {
                        // Apply grain effect with falloff from center
                        float effect = grainIntensity * (1.0f - dist/grainSize);
                        grainPattern[posX][posY] += effect;
                    }
                }
            }
        }
    }
    
    // Now generate the actual texture pixels with the grain pattern influence
    for (uint32_t y = 0; y < textureHeight; ++y) {
        for (uint32_t x = 0; x < textureWidth; ++x) {
            uint32_t offset = (y * textureWidth + x) * 4;
            
            // Base sand color (sandy beige)
            uint8_t r = 220;
            uint8_t g = 185;
            uint8_t b = 140;
            
            // Apply fine noise for microtexture
            uint8_t microNoise = (uint8_t)((rand() % 20) - 10);
            
            // Apply grain pattern influence
            float grainValue = bx::clamp(grainPattern[x][y], 0.0f, 1.0f);
            float darkening = 1.0f - (grainValue * 0.3f);  // Darken based on grain pattern
            
            // Create slight horizontal bands for a natural layered sand look
            bool isDarkerBand = ((y / 12) % 2) == 0;
            float bandFactor = isDarkerBand ? 0.9f : 1.0f;
            
            // Apply all color adjustments
            r = (uint8_t)bx::clamp((r * darkening * bandFactor) + microNoise, 160.0f, 240.0f);
            g = (uint8_t)bx::clamp((g * darkening * bandFactor) + microNoise, 130.0f, 210.0f);
            b = (uint8_t)bx::clamp((b * darkening * bandFactor) + microNoise, 100.0f, 170.0f);
            
            // Store the final color
            data[offset + 0] = r; // R
            data[offset + 1] = g; // G
            data[offset + 2] = b; // B
            data[offset + 3] = 255; // A
        }
    }
    
    std::cout << "Created procedural sand texture" << std::endl;
    
    // Create texture
    uint64_t textureFlags = BGFX_TEXTURE_NONE;
    textureFlags |= BGFX_SAMPLER_MIN_ANISOTROPIC;  // Better minification filter (for distant views)
    textureFlags |= BGFX_SAMPLER_MAG_ANISOTROPIC;  // Better magnification filter (for close-up views)
    
    bgfx::TextureHandle texture = bgfx::createTexture2D(
        textureWidth,
        textureHeight,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        textureFlags,
        texMem
    );
    
    // Load the PNG texture for the third cube
    std::cout << "Loading PNG texture for the third cube..." << std::endl;
    const char* pngTexturePath = "assets/sandy_gravel_02_diff_1k.png";
    png_texture = load_png_texture(pngTexturePath);
    
    if (!bgfx::isValid(png_texture)) {
        std::cerr << "Failed to create PNG texture, third cube will not be rendered" << std::endl;
    } else {
        std::cout << "PNG texture loaded successfully!" << std::endl;
    }
    
    if (!bgfx::isValid(texture)) {
        std::cerr << "Failed to create texture!" << std::endl;
        return 1;
    }
    
    std::cout << "Texture created successfully!" << std::endl;
    
    // Create texture uniforms
    bgfx::UniformHandle s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    png_s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);  // We can reuse the same name as shaders are separate
    
    // Load the Spartan GLB model
    std::cout << "Loading Spartan GLB model..." << std::endl;
    const char* modelPath = "assets/spartan_halo.glb";
    if (!spartanModel.loadFromFile(modelPath)) {
        std::cerr << "Failed to load Spartan model!" << std::endl;
        // Continue without the model
    } else {
        std::cout << "Spartan model loaded successfully!" << std::endl;
    }
    
    // Load colored cube shaders
    std::cout << "Loading shaders..." << std::endl;
    
    // Metal shaders for macOS
    bgfx::ShaderHandle vsh;
    bgfx::ShaderHandle fsh;
    bgfx::ShaderHandle texVsh;
    bgfx::ShaderHandle texFsh;
    
#if BX_PLATFORM_OSX
    // Colored cube shaders
    std::string vsPath = "shaders/metal/vs_cube.bin";
    std::string fsPath = "shaders/metal/fs_cube.bin";
    
    // Textured cube shaders
    std::string texVsPath = "shaders/metal/vs_textured_cube.bin";
    std::string texFsPath = "shaders/metal/fs_textured_cube.bin";
    
    std::cout << "Looking for shaders at:" << std::endl;
    std::cout << "  Colored cube vertex: " << vsPath << std::endl;
    std::cout << "  Colored cube fragment: " << fsPath << std::endl;
    std::cout << "  Textured cube vertex: " << texVsPath << std::endl;
    std::cout << "  Textured cube fragment: " << texFsPath << std::endl;
    
    // Open colored cube shader files
    FILE* vsFile = fopen(vsPath.c_str(), "rb");
    FILE* fsFile = fopen(fsPath.c_str(), "rb");
    
    // Open textured cube shader files
    FILE* texVsFile = fopen(texVsPath.c_str(), "rb");
    FILE* texFsFile = fopen(texFsPath.c_str(), "rb");
    
    // Check if all files opened successfully
    if (!vsFile || !fsFile || !texVsFile || !texFsFile) {
        if (vsFile) fclose(vsFile);
        if (fsFile) fclose(fsFile);
        if (texVsFile) fclose(texVsFile);
        if (texFsFile) fclose(texFsFile);
        std::cerr << "Failed to open shader files!" << std::endl;
        return 1;
    }
    
    // Get file sizes
    fseek(vsFile, 0, SEEK_END);
    fseek(fsFile, 0, SEEK_END);
    fseek(texVsFile, 0, SEEK_END);
    fseek(texFsFile, 0, SEEK_END);
    
    long vsSize = ftell(vsFile);
    long fsSize = ftell(fsFile);
    long texVsSize = ftell(texVsFile);
    long texFsSize = ftell(texFsFile);
    
    fseek(vsFile, 0, SEEK_SET);
    fseek(fsFile, 0, SEEK_SET);
    fseek(texVsFile, 0, SEEK_SET);
    fseek(texFsFile, 0, SEEK_SET);
    
    // Read the shader files
    const bgfx::Memory* vs_mem = bgfx::alloc(vsSize);
    const bgfx::Memory* fs_mem = bgfx::alloc(fsSize);
    const bgfx::Memory* texVs_mem = bgfx::alloc(texVsSize);
    const bgfx::Memory* texFs_mem = bgfx::alloc(texFsSize);
    
    size_t vsRead = fread(vs_mem->data, 1, vsSize, vsFile);
    size_t fsRead = fread(fs_mem->data, 1, fsSize, fsFile);
    size_t texVsRead = fread(texVs_mem->data, 1, texVsSize, texVsFile);
    size_t texFsRead = fread(texFs_mem->data, 1, texFsSize, texFsFile);
    
    fclose(vsFile);
    fclose(fsFile);
    fclose(texVsFile);
    fclose(texFsFile);
    
    // Verify all files were read correctly
    if (vsRead != vsSize || fsRead != fsSize || texVsRead != texVsSize || texFsRead != texFsSize) {
        std::cerr << "Failed to read shader files fully!" << std::endl;
        return 1;
    }
    
    // Create shader objects
    std::cout << "Creating shaders..." << std::endl;
    vsh = bgfx::createShader(vs_mem);
    fsh = bgfx::createShader(fs_mem);
    texVsh = bgfx::createShader(texVs_mem);
    texFsh = bgfx::createShader(texFs_mem);
    
    // Verify all shaders were created successfully
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh) || !bgfx::isValid(texVsh) || !bgfx::isValid(texFsh)) {
        std::cerr << "Failed to create shader objects!" << std::endl;
        return 1;
    }
#else
    // For other platforms (not implemented in this example)
    std::cerr << "Shaders for this platform are not implemented in this example." << std::endl;
    return 1;
#endif

    // Create shader programs
    std::cout << "Creating shader programs..." << std::endl;
    bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
    bgfx::ProgramHandle texProgram = bgfx::createProgram(texVsh, texFsh, true);
    
    // Use the same shaders for the PNG textured cube as the procedural textured cube
    png_texProgram = bgfx::createProgram(texVsh, texFsh, true);
    
    if (!bgfx::isValid(program) || !bgfx::isValid(texProgram) || !bgfx::isValid(png_texProgram)) {
        std::cerr << "Failed to create shader programs!" << std::endl;
        return 1;
    }
    
    // Create vertex and index buffers for the PNG textured cube (reusing the same data as the second cube)
    png_texVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(texCubeVertices, sizeof(texCubeVertices)),
        texLayout
    );
    
    png_texIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(texCubeIndices, sizeof(texCubeIndices))
    );
    
    std::cout << "Starting main loop..." << std::endl;
    std::cout << "===== Controls =====" << std::endl;
    std::cout << "WASD - Move camera" << std::endl;
    std::cout << "SHIFT - Sprint (faster movement)" << std::endl;
    std::cout << "Q/E  - Move up/down" << std::endl;
    std::cout << "Click and drag mouse - Rotate camera" << std::endl;
    std::cout << "ESC  - Exit" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Main game loop variables
    bool quit = false;
    SDL_Event event;
    float time = 0.0f;
    
    // Camera variables
    bx::Vec3 cameraPos = {0.0f, 0.0f, -10.0f}; // Initial camera position
    float cameraYaw = 0.0f;                    // Camera rotation around Y axis (left/right)
    float cameraPitch = 0.0f;                  // Camera rotation around X axis (up/down)
    
    // Mouse variables for camera rotation
    float prevMouseX = 0.0f;
    float prevMouseY = 0.0f;
    bool mouseDown = false;
    
    // Keyboard state for continuous movement
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
            // Handle window resize events
            else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                int width = event.window.data1;
                int height = event.window.data2;
                bgfx::reset(width, height, BGFX_RESET_VSYNC);
                bgfx::setViewRect(0, 0, 0, width, height);
            }
            // Handle mouse button events for camera rotation
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouseDown = true;
                    SDL_GetMouseState(&prevMouseX, &prevMouseY);
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouseDown = false;
                }
            }
            // Handle mouse motion events for camera rotation
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (mouseDown) {
                    float mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    
                    // Calculate mouse delta
                    float deltaX = mouseX - prevMouseX;
                    float deltaY = mouseY - prevMouseY;
                    
                    // Update camera rotation angles
                    cameraYaw += deltaX * CAMERA_ROTATE_SPEED;
                    cameraPitch += deltaY * CAMERA_ROTATE_SPEED;
                    
                    // Limit pitch to avoid flipping the camera
                    if (cameraPitch > bx::kPi * 0.49f) cameraPitch = bx::kPi * 0.49f;
                    if (cameraPitch < -bx::kPi * 0.49f) cameraPitch = -bx::kPi * 0.49f;
                    
                    // Update previous mouse position
                    prevMouseX = mouseX;
                    prevMouseY = mouseY;
                }
            }
        }
        
        // Get updated keyboard state for continuous movement
        keyboardState = SDL_GetKeyboardState(NULL);
        
        // Process WASD movement
        // First calculate the forward and right vectors based on camera rotation
        float forward[3] = { 
            bx::sin(cameraYaw) * bx::cos(cameraPitch),
            -bx::sin(cameraPitch),
            bx::cos(cameraYaw) * bx::cos(cameraPitch)
        };
        
        float right[3] = {
            bx::cos(cameraYaw),
            0.0f,
            -bx::sin(cameraYaw)
        };
        
        // Check if Shift key is pressed for sprint mode
        bool sprinting = keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT];
        float currentSpeed = CAMERA_MOVE_SPEED * (sprinting ? CAMERA_SPRINT_MULTIPLIER : 1.0f);
        
        // Move camera based on keyboard input
        if (keyboardState[SDL_SCANCODE_W]) {
            // Move forward
            cameraPos.x += forward[0] * currentSpeed;
            cameraPos.y += forward[1] * currentSpeed;
            cameraPos.z += forward[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_S]) {
            // Move backward
            cameraPos.x -= forward[0] * currentSpeed;
            cameraPos.y -= forward[1] * currentSpeed;
            cameraPos.z -= forward[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_A]) {
            // Move left
            cameraPos.x -= right[0] * currentSpeed;
            cameraPos.y -= right[1] * currentSpeed;
            cameraPos.z -= right[2] * currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_D]) {
            // Move right
            cameraPos.x += right[0] * currentSpeed;
            cameraPos.y += right[1] * currentSpeed;
            cameraPos.z += right[2] * currentSpeed;
        }
        // Add up/down movement with Q and E
        if (keyboardState[SDL_SCANCODE_Q]) {
            // Move up
            cameraPos.y += currentSpeed;
        }
        if (keyboardState[SDL_SCANCODE_E]) {
            // Move down
            cameraPos.y -= currentSpeed;
        }
        
        // Update timing for animation
        time += 0.01f;

        // Set up camera view matrix based on camera position and rotation
        // Calculate the "look at" point based on camera rotation
        bx::Vec3 target = {
            cameraPos.x + forward[0],
            cameraPos.y + forward[1],
            cameraPos.z + forward[2]
        };
        
        // Create the view matrix
        float view[16];
        bx::mtxLookAt(view, cameraPos, target);
        
        float proj[16];
        bx::mtxProj(proj,
                   60.0f,
                   float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
                   0.1f,
                   100.0f,
                   bgfx::getCaps()->homogeneousDepth);
        
        bgfx::setViewTransform(0, view, proj);
        bgfx::setViewRect(0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        // ------------------------------
        // Render colored cube (left side)
        // ------------------------------
        
        // Create transform matrix for colored cube (left side)
        float coloredMtx[16];
        float translation[16];
        
        // Position the colored cube on the left
        bx::mtxTranslate(translation, -2.5f, 0.0f, 0.0f);
        
        // Apply rotation
        float rotation[16];
        bx::mtxRotateXY(rotation, time * 0.21f, time * 0.37f);
        
        // Combine rotation and translation
        bx::mtxMul(coloredMtx, rotation, translation);
        
        // Set model matrix
        bgfx::setTransform(coloredMtx);
        
        // Set vertex and index buffers
        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);
        
        // Set render states - disable backface culling to ensure all faces are visible
        uint64_t coloredState = BGFX_STATE_DEFAULT;
        coloredState &= ~BGFX_STATE_CULL_MASK;   // Remove any culling flags
        bgfx::setState(coloredState);
        
        // Submit colored cube for rendering
        bgfx::submit(0, program);
        
        // ------------------------------
        // Render textured cube (right side)
        // ------------------------------
        
        // Create transform matrix for textured cube (right side)
        float texturedMtx[16];
        
        // Position the textured cube on the right
        bx::mtxTranslate(translation, 2.5f, 0.0f, 0.0f);
        
        // Apply rotation, but in opposite direction for visual interest
        bx::mtxRotateXY(rotation, time * -0.21f, time * -0.37f);
        
        // Combine rotation and translation
        bx::mtxMul(texturedMtx, rotation, translation);
        
        // Set model matrix
        bgfx::setTransform(texturedMtx);
        
        // Set texture (no additional flags needed here as they're set when creating the texture)
        bgfx::setTexture(0, s_texColor, texture);
        
        // Set vertex and index buffers
        bgfx::setVertexBuffer(0, texVbh);
        bgfx::setIndexBuffer(texIbh);
        
        // Set render states - disable backface culling to ensure all faces are visible
        uint64_t texturedState = BGFX_STATE_DEFAULT;
        texturedState &= ~BGFX_STATE_CULL_MASK;   // Remove any culling flags
        bgfx::setState(texturedState);
        
        // Submit textured cube for rendering
        bgfx::submit(0, texProgram);
        
        // Only render the third cube if we have a valid PNG texture
        if (bgfx::isValid(png_texture)) {
            // ------------------------------
            // Render PNG textured cube (top)
            // ------------------------------
            
            // Create transform matrix for PNG textured cube
            float pngTexMtx[16];
            
            // Position the PNG textured cube on top
            bx::mtxTranslate(translation, 0.0f, 2.5f, 0.0f);
            
            // Apply rotation with different speeds for visual interest
            bx::mtxRotateXY(rotation, time * 0.15f, time * 0.3f);
            
            // Combine rotation and translation
            bx::mtxMul(pngTexMtx, rotation, translation);
            
            // Set model matrix
            bgfx::setTransform(pngTexMtx);
            
            // Set texture
            bgfx::setTexture(0, png_s_texColor, png_texture);
            
            // Set vertex and index buffers
            bgfx::setVertexBuffer(0, png_texVbh);
            bgfx::setIndexBuffer(png_texIbh);
            
            // Set render states - disable backface culling to ensure all faces are visible
            uint64_t pngTexState = BGFX_STATE_DEFAULT;
            pngTexState &= ~BGFX_STATE_CULL_MASK;   // Remove any culling flags
            bgfx::setState(pngTexState);
            
            // Submit PNG textured cube for rendering
            bgfx::submit(0, png_texProgram);
        }
        
        // Render the Spartan model (if loaded)
        if (!spartanModel.meshes.empty()) {
            // ------------------------------
            // Render Spartan model
            // ------------------------------
            
            // Create transform matrix for the model
            float modelMatrix[16];
            
            // Position it in front of the camera, a bit to the right
            bx::mtxIdentity(modelMatrix);
            
            // Scale down the model (it's likely much larger than our cubes)
            float scale[16];
            bx::mtxScale(scale, 0.05f, 0.05f, 0.05f);  // Adjust scale as needed
            
            // Rotation to face the camera
            float rotation[16];
            bx::mtxRotateY(rotation, time * 0.5f);  // Slow rotation around Y axis
            
            // Translation
            float translation[16];
            bx::mtxTranslate(translation, 5.0f, -1.0f, 0.0f);  // To the right
            
            // Combine transformations: scale -> rotate -> translate
            float temp[16];
            bx::mtxMul(temp, scale, rotation);
            bx::mtxMul(modelMatrix, temp, translation);
            
            // Render the model
            spartanModel.render(texProgram, s_texColor, modelMatrix);
        }
        
        // Advance frame
        bgfx::frame();
    }
    
    // Clean up resources
    bgfx::destroy(texture);
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
    
    // Clean up PNG textured cube resources
    if (bgfx::isValid(png_texture)) {
        bgfx::destroy(png_texture);
        bgfx::destroy(png_s_texColor);
        bgfx::destroy(png_texIbh);
        bgfx::destroy(png_texVbh);
        bgfx::destroy(png_texProgram);
    }
    
    // Clean up Spartan model resources
    spartanModel.unload();
    
    // Shutdown BGFX
    bgfx::shutdown();
    
    // Clean up SDL
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}