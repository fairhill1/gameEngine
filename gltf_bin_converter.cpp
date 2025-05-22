#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <memory>

// Include TinyGLTF
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// Define our expected output vertex structure (must match the game's definition)
struct PosNormalTexcoordVertex {
    float position[3];
    uint32_t normal;
    int16_t texcoord[2];
};

// Helper function to convert normals to packed format
uint32_t encodeNormalRgba8(float _x, float _y, float _z) {
    // Normalize the vector
    float length = sqrtf(_x*_x + _y*_y + _z*_z);
    float nx = length > 0.0f ? _x / length : 0.0f;
    float ny = length > 0.0f ? _y / length : 0.0f;
    float nz = length > 0.0f ? _z / length : 0.0f;
    
    // Map from [-1, 1] to [0, 255]
    uint8_t xyz[4];
    xyz[0] = uint8_t(nx * 127.5f + 127.5f);
    xyz[1] = uint8_t(ny * 127.5f + 127.5f);
    xyz[2] = uint8_t(nz * 127.5f + 127.5f);
    xyz[3] = 0; // Unused
    
    return xyz[0] | (xyz[1] << 8) | (xyz[2] << 16) | (xyz[3] << 24);
}

// Helper function to clamp values
float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_gltf_file> <output_binary_file>" << std::endl;
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    std::cout << "Converting GLTF file: " << inputFile << std::endl;
    std::cout << "Output binary file: " << outputFile << std::endl;

    // Setup tinygltf loader
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // Determine if the input file is GLB or GLTF
    std::string inputPath(inputFile);
    bool result = false;
    
    if (inputPath.find(".glb") != std::string::npos) {
        result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, inputFile);
    } else {
        result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, inputFile);
    }

    // Check for warnings and errors
    if (!warn.empty()) std::cout << "glTF warning: " << warn << std::endl;
    if (!err.empty()) {
        std::cerr << "glTF error: " << err << std::endl;
        return 1;
    }
    if (!result) {
        std::cerr << "Failed to load glTF model." << std::endl;
        return 1;
    }

    // Make sure we have at least one mesh
    if (gltfModel.meshes.empty()) {
        std::cerr << "No meshes found in the model." << std::endl;
        return 1;
    }

    // We'll only process the first mesh primitive
    const auto& mesh = gltfModel.meshes[0];
    if (mesh.primitives.empty()) {
        std::cerr << "No primitives found in the mesh." << std::endl;
        return 1;
    }

    const auto& primitive = mesh.primitives[0];

    // Find relevant attribute accessors
    auto positionIt = primitive.attributes.find("POSITION");
    auto normalIt = primitive.attributes.find("NORMAL");
    auto texcoordIt = primitive.attributes.find("TEXCOORD_0");

    // Make sure we have position data
    if (positionIt == primitive.attributes.end()) {
        std::cerr << "Mesh missing position data, exiting" << std::endl;
        return 1;
    }

    // Get accessors and data
    const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionIt->second];
    const tinygltf::BufferView& positionView = gltfModel.bufferViews[positionAccessor.bufferView];
    const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionView.buffer];

    // Prepare vertex data
    size_t vertexCount = positionAccessor.count;
    std::vector<PosNormalTexcoordVertex> vertices(vertexCount);
    std::cout << "Processing " << vertexCount << " vertices..." << std::endl;

    // Extract position data
    for (size_t i = 0; i < vertexCount; i++) {
        size_t offset = positionView.byteOffset + positionAccessor.byteOffset + i * 12; // 3 floats * 4 bytes
        const float* pos = reinterpret_cast<const float*>(&positionBuffer.data[offset]);
        
        // No scaling applied (keep original size)
        vertices[i].position[0] = pos[0];
        vertices[i].position[1] = pos[1];
        vertices[i].position[2] = pos[2];
        
        // Default values
        vertices[i].normal = encodeNormalRgba8(0.0f, 1.0f, 0.0f); // Default normal (0,1,0)
        vertices[i].texcoord[0] = 0; // Default UV (0,0)
        vertices[i].texcoord[1] = 0;
    }

    // Extract normal data if available
    if (normalIt != primitive.attributes.end()) {
        const tinygltf::Accessor& normalAccessor = gltfModel.accessors[normalIt->second];
        const tinygltf::BufferView& normalView = gltfModel.bufferViews[normalAccessor.bufferView];
        const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalView.buffer];
        
        for (size_t i = 0; i < vertexCount; i++) {
            size_t offset = normalView.byteOffset + normalAccessor.byteOffset + i * 12;
            const float* normal = reinterpret_cast<const float*>(&normalBuffer.data[offset]);
            
            // Pack normals into efficient RGBA8 format
            vertices[i].normal = encodeNormalRgba8(normal[0], normal[1], normal[2]);
        }
    }

    // Extract texture coordinates if available
    if (texcoordIt != primitive.attributes.end()) {
        const tinygltf::Accessor& texcoordAccessor = gltfModel.accessors[texcoordIt->second];
        const tinygltf::BufferView& texcoordView = gltfModel.bufferViews[texcoordAccessor.bufferView];
        const tinygltf::Buffer& texcoordBuffer = gltfModel.buffers[texcoordView.buffer];
        
        for (size_t i = 0; i < vertexCount; i++) {
            size_t offset = texcoordView.byteOffset + texcoordAccessor.byteOffset + i * 8;
            const float* texcoord = reinterpret_cast<const float*>(&texcoordBuffer.data[offset]);
            
            // Convert floating point UVs to compressed Int16 format
            vertices[i].texcoord[0] = int16_t(clamp(texcoord[0], 0.0f, 1.0f) * 32767.0f);
            vertices[i].texcoord[1] = int16_t(clamp(texcoord[1], 0.0f, 1.0f) * 32767.0f);
        }
    }

    // Extract indices if present
    std::vector<uint16_t> indices;
    if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = gltfModel.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexView.buffer];
        
        uint32_t indexCount = indexAccessor.count;
        indices.resize(indexCount);
        std::cout << "Processing " << indexCount << " indices..." << std::endl;
        
        // Handle different index types
        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            // Already uint16_t
            for (size_t i = 0; i < indexCount; i++) {
                size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i * 2;
                indices[i] = *reinterpret_cast<const uint16_t*>(&indexBuffer.data[offset]);
            }
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            // Convert uint32_t to uint16_t
            for (size_t i = 0; i < indexCount; i++) {
                size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i * 4;
                uint32_t index = *reinterpret_cast<const uint32_t*>(&indexBuffer.data[offset]);
                
                // Ensure index fits in uint16_t
                if (index > UINT16_MAX) {
                    std::cerr << "Warning: Index " << index << " exceeds uint16_t range, clamping." << std::endl;
                    index = UINT16_MAX;
                }
                indices[i] = static_cast<uint16_t>(index);
            }
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            // Convert uint8_t to uint16_t
            for (size_t i = 0; i < indexCount; i++) {
                size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i;
                uint8_t index = indexBuffer.data[offset];
                indices[i] = static_cast<uint16_t>(index);
            }
        } else {
            std::cerr << "Unsupported index type: " << indexAccessor.componentType << std::endl;
            return 1;
        }
    } else {
        // No indices, create sequential indices
        indices.resize(vertexCount);
        for (uint16_t i = 0; i < vertexCount; i++) {
            indices[i] = i;
        }
    }

    // Write the data to our custom binary format
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open output file: " << outputFile << std::endl;
        return 1;
    }

    // Write header: vertex count and index count
    uint32_t vertexCount32 = static_cast<uint32_t>(vertices.size());
    uint32_t indexCount32 = static_cast<uint32_t>(indices.size());
    outFile.write(reinterpret_cast<const char*>(&vertexCount32), sizeof(uint32_t));
    outFile.write(reinterpret_cast<const char*>(&indexCount32), sizeof(uint32_t));

    // Write vertex data
    outFile.write(reinterpret_cast<const char*>(vertices.data()), 
                 vertices.size() * sizeof(PosNormalTexcoordVertex));

    // Write index data
    outFile.write(reinterpret_cast<const char*>(indices.data()), 
                 indices.size() * sizeof(uint16_t));

    outFile.close();

    std::cout << "Conversion complete!" << std::endl;
    std::cout << "Wrote " << vertices.size() << " vertices and " 
              << indices.size() << " indices to " << outputFile << std::endl;

    return 0;
}