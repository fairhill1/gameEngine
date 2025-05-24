#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declarations
namespace tinygltf {
    class Model;
    class Image;
    class Material;
    class Primitive;
    class Accessor;
    class BufferView;
    class Buffer;
    class TinyGLTF;
}

// Vertex structure matching BGFX examples for proper texture mapping
struct PosNormalTexcoordVertex {
    float position[3];
    uint32_t normal;     // Packed normal (RGBA8 format)
    int16_t texcoord[2]; // Normalized texture coordinates
    uint8_t boneIndices[4]; // Bone indices (up to 4 bones per vertex)
    float boneWeights[4];    // Bone weights (must sum to 1.0)

    static void init() {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};

// Animation keyframe data
struct AnimationKeyframe {
    float time;
    std::vector<float> values; // Translation, rotation, or scale values
};

// Animation channel (position, rotation, scale for a node)
struct AnimationChannel {
    int nodeIndex;
    std::string path; // "translation", "rotation", "scale"
    std::vector<AnimationKeyframe> keyframes;
};

// Animation clip
struct AnimationClip {
    std::string name;
    float duration;
    std::vector<AnimationChannel> channels;
};

// Bone/Joint data
struct Joint {
    int index;
    std::string name;
    int parentIndex; // -1 for root
    float bindMatrix[16]; // Inverse bind matrix
    float localMatrix[16]; // Local transform matrix
    float globalMatrix[16]; // Global transform matrix
    std::vector<int> children;
};

// Skinning data
struct Skin {
    std::vector<Joint> joints;
    std::vector<int> jointIndices; // Maps to joints array
};

// Simple mesh structure
struct ModelMesh {
    bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
    int indexCount = 0;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    int primitiveType = 4; // TINYGLTF_MODE_TRIANGLES (4) is the default
    
    // For animation
    std::vector<PosNormalTexcoordVertex> originalVertices; // Bind pose vertices
    std::vector<PosNormalTexcoordVertex> animatedVertices; // After bone transforms
    bool hasAnimation = false;
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
    
    // Render multiple instances of the model
    void renderInstanced(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform, 
                        bgfx::InstanceDataBuffer* instanceBuffer, uint32_t instanceCount);
    
    // Free resources
    void unload();
    
    // Check if the model has any meshes loaded
    bool hasAnyMeshes() const { return !meshes.empty(); }
    
    // Set a fallback texture to use when meshes don't have their own texture
    void setFallbackTexture(bgfx::TextureHandle texture) { fallbackTexture = texture; }
    
    // Update model vertices with ozz animation bone matrices  
    void updateWithOzzBoneMatrices(const float* boneMatrices, size_t boneCount);
    
    // Update model vertices using ozz native skinning
    void updateWithOzzSkinning(class OzzAnimationSystem& ozzSystem);
    
    // Get inverse bind matrices for ozz skinning setup
    bool getInverseBindMatrices(std::vector<float>& outMatrices) const;
    
    // Remap vertex bone indices from glTF space to ozz space
    void remapBoneIndices(const std::vector<int>& gltfToOzzMapping);
    
    // Animation support
    bool hasAnimations() const { return !animations.empty(); }
    const std::vector<AnimationClip>& getAnimations() const { return animations; }
    const AnimationClip* getAnimation(const std::string& name) const;
    
    // Bone animation
    void calculateBoneMatrices(const std::string& animationName, float time, std::vector<float*>& boneMatrices) const;
    void updateNodeMatrix(int nodeIndex, const std::string& animationName, float time);
    void updateAnimatedVertices(const std::string& animationName, float time);
    
    // Direct binary mesh processing for testing
    bool processBinaryMesh(const std::vector<uint8_t>& data);
    
private:
    // Helper functions for loading
    bool processGltfModel(const tinygltf::Model& gltfModel);
    
    // Helper function to convert floats to packed representation
    static uint32_t encodeNormalRgba8(float _x, float _y, float _z);
    
    // Validation functions for debugging
    static bool validateVertexData(const std::vector<PosNormalTexcoordVertex>& vertices, const std::string& meshName);
    static bool validateTextureCoordinates(const std::vector<PosNormalTexcoordVertex>& vertices);
    static bool validateNormals(const std::vector<PosNormalTexcoordVertex>& vertices);
    
public:
    // Public fields for direct access
    std::vector<ModelMesh> meshes;
    bgfx::TextureHandle fallbackTexture = BGFX_INVALID_HANDLE;
    
    // Animation data
    std::vector<AnimationClip> animations;
    
    // Skinning data
    std::vector<Skin> skins;
    std::vector<Joint> nodes; // All nodes (including non-joint nodes)
    
private:
    
    // Cache of loaded textures by source index
    std::unordered_map<int, bgfx::TextureHandle> loadedTextures;
};