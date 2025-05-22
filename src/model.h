#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declarations
namespace tinygltf {
    class Model;
}

// Simple vertex structure for models
struct PosNormalTexcoordVertex {
    float position[3];
    uint32_t normal;
    int16_t texcoord[2];

    static void init() {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};

// Simple mesh structure
struct ModelMesh {
    bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
    int indexCount = 0;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    int primitiveType = 4; // TINYGLTF_MODE_TRIANGLES (4) is the default
};

// Simplified model class focused on GLTF loading
class Model {
public:
    Model() = default;
    ~Model();
    
    // Initialize vertex layout - call before using any Model objects
    static void init();
    
    // Load a model from GLTF/GLB file
    bool loadFromFile(const char* filepath);
    
    // Load a model directly from a binary mesh file
    bool loadFromBinary(const char* filepath);
    
    // Render the model with the given program and transform
    void render(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform, float* modelMatrix);
    
    // Free resources
    void unload();
    
    // Check if the model has any meshes loaded
    bool hasAnyMeshes() const { return !meshes.empty(); }
    
    // Set a fallback texture to use when meshes don't have their own texture
    void setFallbackTexture(bgfx::TextureHandle texture) { fallbackTexture = texture; }
    
    // Direct binary mesh processing for testing
    bool processBinaryMesh(const std::vector<uint8_t>& data);
    
private:
    // Helper functions for loading
    bool processGltfModel(const tinygltf::Model& gltfModel);
    
    // Helper function to convert floats to packed representation
    static uint32_t encodeNormalRgba8(float _x, float _y, float _z);
    
public:
    // Public fields for direct access
    std::vector<ModelMesh> meshes;
    bgfx::TextureHandle fallbackTexture = BGFX_INVALID_HANDLE;
    
private:
    
    // Cache of loaded textures by source index
    std::unordered_map<int, bgfx::TextureHandle> loadedTextures;
};