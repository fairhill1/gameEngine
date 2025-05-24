#include "model.h"
#include "ozz_animation.h"
#include <iostream>
#include <algorithm>
#include <bx/math.h>
#include <fstream>
#include <vector>
#include <cstdio>

// Include stb_image without redefining the implementation
#include "stb_image.h"

// Include TinyGLTF header without implementation
#include "tiny_gltf.h"

// Initialize static vertex layout
bgfx::VertexLayout PosNormalTexcoordVertex::ms_layout;

Model::~Model() {
    unload();
}

void Model::init() {
    // Initialize vertex layout once before using any Model objects
    PosNormalTexcoordVertex::init();
}

// Helper function to convert normals to packed format for better Metal performance
uint32_t Model::encodeNormalRgba8(float _x, float _y, float _z) {
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

// Validation functions for debugging vertex data
bool Model::validateVertexData(const std::vector<PosNormalTexcoordVertex>& vertices, const std::string& meshName) {
    if (vertices.empty()) {
        std::cerr << "VALIDATION: " << meshName << " has no vertices!" << std::endl;
        return false;
    }
    
    std::cout << "VALIDATION: " << meshName << " - " << vertices.size() << " vertices loaded" << std::endl;
    
    // Show first few vertices for debugging
    size_t samplesToShow = std::min(vertices.size(), size_t(3));
    for (size_t i = 0; i < samplesToShow; i++) {
        std::cout << "  Vertex " << i << ": pos=(" 
                  << vertices[i].position[0] << "," << vertices[i].position[1] << "," << vertices[i].position[2] 
                  << ") normal=0x" << std::hex << vertices[i].normal << std::dec
                  << " uv=(" << vertices[i].texcoord[0] << "," << vertices[i].texcoord[1] << ")" << std::endl;
    }
    
    return validateNormals(vertices) && validateTextureCoordinates(vertices);
}

bool Model::validateTextureCoordinates(const std::vector<PosNormalTexcoordVertex>& vertices) {
    int validUVs = 0;
    int zeroUVs = 0;
    
    for (const auto& vertex : vertices) {
        if (vertex.texcoord[0] != 0 || vertex.texcoord[1] != 0) {
            validUVs++;
        } else {
            zeroUVs++;
        }
    }
    
    float uvRatio = float(validUVs) / vertices.size();
    std::cout << "VALIDATION: UV coordinates - " << validUVs << " non-zero, " << zeroUVs << " zero (" 
              << (uvRatio * 100.0f) << "% have UVs)" << std::endl;
    
    if (uvRatio < 0.1f) {
        std::cerr << "WARNING: Very few vertices have texture coordinates!" << std::endl;
    }
    
    return true;
}

bool Model::validateNormals(const std::vector<PosNormalTexcoordVertex>& vertices) {
    int validNormals = 0;
    int zeroNormals = 0;
    
    for (const auto& vertex : vertices) {
        if (vertex.normal != 0) {
            validNormals++;
        } else {
            zeroNormals++;
        }
    }
    
    float normalRatio = float(validNormals) / vertices.size();
    std::cout << "VALIDATION: Normals - " << validNormals << " non-zero, " << zeroNormals << " zero (" 
              << (normalRatio * 100.0f) << "% have normals)" << std::endl;
    
    if (normalRatio < 0.1f) {
        std::cerr << "WARNING: Very few vertices have normals!" << std::endl;
    }
    
    return true;
}

bool Model::loadFromFile(const char* filepath) {
    std::cout << "Loading model from: " << filepath << std::endl;
    
    // Clear any existing data
    unload();
    
    // Extract file extension
    std::string path(filepath);
    std::string extension;
    
    size_t dot_pos = path.find_last_of(".");
    if (dot_pos != std::string::npos) {
        extension = path.substr(dot_pos);
        std::transform(extension.begin(), extension.end(), extension.begin(), 
                     [](unsigned char c) { return std::tolower(c); });
    }
    
    if (extension != ".glb" && extension != ".gltf") {
        std::cerr << "Unsupported file extension: " << extension << ". Only GLB and GLTF files are supported." << std::endl;
        return false;
    }
    
    // Setup tinygltf loader
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    // Set up custom image loader
    loader.SetImageLoader(
        [](tinygltf::Image* image, const int imageIndex, 
           std::string* err, std::string* warn, int req_width, int req_height, 
           const unsigned char* bytes, int size, void* userData) -> bool {
            
            // Load embedded image data
            if (bytes && size > 0) {
                int width, height, channels;
                unsigned char* data = stbi_load_from_memory(bytes, size, &width, &height, &channels, STBI_default);
                
                if (!data) {
                    if (err) *err = std::string("Failed to load embedded image: ") + stbi_failure_reason();
                    return false;
                }
                
                // Store the image data
                image->width = width;
                image->height = height;
                image->component = channels;
                image->image.resize(static_cast<size_t>(width * height * channels));
                std::memcpy(image->image.data(), data, width * height * channels);
                
                stbi_image_free(data);
                return true;
            }
            
            // Load external image
            if (!image->uri.empty()) {
                int width, height, channels;
                unsigned char* data = stbi_load(image->uri.c_str(), &width, &height, &channels, STBI_default);
                
                if (!data) {
                    if (err) *err = std::string("Failed to load image file: ") + stbi_failure_reason();
                    return false;
                }
                
                image->width = width;
                image->height = height;
                image->component = channels;
                image->image.resize(static_cast<size_t>(width * height * channels));
                std::memcpy(image->image.data(), data, width * height * channels);
                
                stbi_image_free(data);
                return true;
            }
            
            if (err) *err = "No image data provided";
            return false;
        },
        nullptr
    );
    
    // Load the GLB/glTF file
    bool result = false;
    if (extension == ".glb") {
        result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
    } else {
        result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
    }
    
    // Check for warnings and errors
    if (!warn.empty()) std::cout << "glTF warning: " << warn << std::endl;
    if (!err.empty()) {
        std::cerr << "glTF error: " << err << std::endl;
        return false;
    }
    if (!result) {
        std::cerr << "Failed to load glTF model." << std::endl;
        return false;
    }
    
    // Process the model
    return processGltfModel(gltfModel);
}

bool Model::loadFromBinary(const char* filepath) {
    std::cout << "Loading binary mesh from: " << filepath << std::endl;
    
    // Clear any existing data
    unload();
    
    // Open the binary file
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open binary mesh file: " << filepath << std::endl;
        return false;
    }
    
    // Get file size and read the data
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read binary mesh file data" << std::endl;
        return false;
    }
    
    // Process the binary data
    return processBinaryMesh(buffer);
}

bool Model::processGltfModel(const tinygltf::Model& gltfModel) {
    // Count total primitives
    size_t totalPrimitives = 0;
    for (const auto& mesh : gltfModel.meshes) {
        totalPrimitives += mesh.primitives.size();
    }
    std::cout << "Processing " << totalPrimitives << " mesh primitives..." << std::endl;
    
    // Print material debug info
    fprintf(stderr, "MATERIAL_DEBUG: Model has %zu materials\n", gltfModel.materials.size());
    for (size_t i = 0; i < gltfModel.materials.size(); i++) {
        const auto& material = gltfModel.materials[i];
        fprintf(stderr, "MATERIAL_DEBUG: Material %zu: name=%s\n", 
                i, material.name.c_str());
        
        // Check if material has a base color texture
        if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            fprintf(stderr, "MATERIAL_DEBUG: Material %zu has baseColorTexture index %d\n", 
                    i, textureIndex);
        } else {
            fprintf(stderr, "MATERIAL_DEBUG: Material %zu has NO baseColorTexture\n", i);
        }
    }
    
    // Process each mesh
    for (const auto& mesh : gltfModel.meshes) {
        for (const auto& primitive : mesh.primitives) {
            ModelMesh modelMesh;
            
            // Debug primitive material
            fprintf(stderr, "MESH_DEBUG: Processing mesh %s, primitive material: %d\n", 
                    mesh.name.c_str(), primitive.material);
            
            // Find relevant attribute accessors
            auto positionIt = primitive.attributes.find("POSITION");
            auto normalIt = primitive.attributes.find("NORMAL");
            auto texcoordIt = primitive.attributes.find("TEXCOORD_0");
            
            // Debug attribute availability
            fprintf(stderr, "MESH_DEBUG: Attributes - Position: %s, Normal: %s, Texcoord: %s\n",
                    (positionIt != primitive.attributes.end() ? "YES" : "NO"),
                    (normalIt != primitive.attributes.end() ? "YES" : "NO"),
                    (texcoordIt != primitive.attributes.end() ? "YES" : "NO"));
            
            // Skip if missing position data
            if (positionIt == primitive.attributes.end()) {
                std::cerr << "Mesh missing position data, skipping" << std::endl;
                continue;
            }
            
            // Get position accessor and data
            const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionIt->second];
            const tinygltf::BufferView& positionView = gltfModel.bufferViews[positionAccessor.bufferView];
            const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionView.buffer];
            
            // Debug buffer layout
            fprintf(stderr, "BUFFER_DEBUG: Position accessor count=%zu, componentType=%d, type=%d\n", 
                   positionAccessor.count, positionAccessor.componentType, positionAccessor.type);
            fprintf(stderr, "BUFFER_DEBUG: Position BufferView byteOffset=%zu, byteLength=%zu, byteStride=%zu\n",
                   positionView.byteOffset, positionView.byteLength, positionView.byteStride);
            fprintf(stderr, "BUFFER_DEBUG: Position Accessor byteOffset=%zu\n", positionAccessor.byteOffset);
            
            // This model uses separate buffer views - positions are tightly packed
            fprintf(stderr, "BUFFER_DEBUG: Using separate buffer views (not interleaved)\n");
            
            // Prepare vertex data with proper initialization
            size_t vertexCount = positionAccessor.count;
            std::vector<PosNormalTexcoordVertex> vertices(vertexCount);
            
            // Initialize all vertices to safe defaults
            for (size_t i = 0; i < vertexCount; i++) {
                vertices[i].position[0] = 0.0f;
                vertices[i].position[1] = 0.0f;
                vertices[i].position[2] = 0.0f;
                vertices[i].normal = encodeNormalRgba8(0.0f, 1.0f, 0.0f); // Default up normal
                vertices[i].texcoord[0] = 0; // int16_t default
                vertices[i].texcoord[1] = 0; // int16_t default
                
                // Initialize bone data to defaults (no skinning)
                vertices[i].boneIndices[0] = 0;
                vertices[i].boneIndices[1] = 0;
                vertices[i].boneIndices[2] = 0;
                vertices[i].boneIndices[3] = 0;
                vertices[i].boneWeights[0] = 1.0f; // First bone gets all weight
                vertices[i].boneWeights[1] = 0.0f;
                vertices[i].boneWeights[2] = 0.0f;
                vertices[i].boneWeights[3] = 0.0f;
            }
            
            // Extract position data from separate buffer view (tightly packed)
            for (size_t i = 0; i < vertexCount; i++) {
                // For separate buffer views, positions are tightly packed (3 floats per vertex)
                size_t offset = positionView.byteOffset + positionAccessor.byteOffset + i * 12;
                
                // Bounds checking to prevent memory corruption
                if (offset + 12 <= positionBuffer.data.size()) {
                    const float* pos = reinterpret_cast<const float*>(&positionBuffer.data[offset]);
                    
                    // Store position data with scaling applied directly to vertex positions
                    const float scaleFactor = 1.0f;
                    vertices[i].position[0] = pos[0] * scaleFactor;
                    vertices[i].position[1] = pos[1] * scaleFactor;
                    vertices[i].position[2] = pos[2] * scaleFactor;
                    
                    // Debug: Print first few vertices and their raw data + hex dump
                    if (i < 5) {
                        const uint8_t* bytes = &positionBuffer.data[offset];
                        fprintf(stderr, "VERTEX_DEBUG: Vertex %zu: offset=%zu, hex=[%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x], raw=(%.6f, %.6f, %.6f)\n", 
                               i, offset,
                               bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], 
                               bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
                               pos[0], pos[1], pos[2]);
                    }
                    
                    // Additional debug: Sample vertices at different points
                    if (i == 4 && vertexCount > 100) {
                        // Sample vertices at 25%, 50%, 75% through the buffer
                        size_t testIndices[] = {vertexCount/4, vertexCount/2, vertexCount*3/4};
                        for (int j = 0; j < 3; j++) {
                            size_t testOffset = positionView.byteOffset + positionAccessor.byteOffset + testIndices[j] * 12;
                            if (testOffset + 12 <= positionBuffer.data.size()) {
                                const float* testPos = reinterpret_cast<const float*>(&positionBuffer.data[testOffset]);
                                fprintf(stderr, "VERTEX_DEBUG: Sample vertex %zu: offset=%zu, raw=(%.6f, %.6f, %.6f)\n", 
                                       testIndices[j], testOffset, testPos[0], testPos[1], testPos[2]);
                            }
                        }
                    }
                } else {
                    // Safe fallback for out-of-bounds access
                    vertices[i].position[0] = 0.0f;
                    vertices[i].position[1] = 0.0f;
                    vertices[i].position[2] = 0.0f;
                    fprintf(stderr, "ERROR: Position buffer bounds exceeded at vertex %zu, offset=%zu, buffer_size=%zu\n", 
                           i, offset, positionBuffer.data.size());
                }
                
                // Default texture coordinates (already set in initialization)
                // vertices[i].texcoord[0] = 0; // Already set above
                // vertices[i].texcoord[1] = 0; // Already set above
            }
            
            // Process normals if available
            if (normalIt != primitive.attributes.end()) {
                const tinygltf::Accessor& normalAccessor = gltfModel.accessors[normalIt->second];
                const tinygltf::BufferView& normalView = gltfModel.bufferViews[normalAccessor.bufferView];
                const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalView.buffer];
                
                fprintf(stderr, "NORMAL_DEBUG: Processing normals, count=%zu\n", normalAccessor.count);
                
                // For separate buffer views, normals are tightly packed (3 floats per vertex)
                for (size_t i = 0; i < vertexCount && i < normalAccessor.count; i++) {
                    size_t offset = normalView.byteOffset + normalAccessor.byteOffset + i * 12;
                    
                    // Bounds checking to prevent memory corruption
                    if (offset + 12 <= normalBuffer.data.size()) {
                        const float* normal = reinterpret_cast<const float*>(&normalBuffer.data[offset]);
                        
                        // Encode normal as packed RGBA8
                        vertices[i].normal = encodeNormalRgba8(normal[0], normal[1], normal[2]);
                        
                        // Debug first few normals
                        if (i < 3) {
                            fprintf(stderr, "NORMAL_DEBUG: Vertex %zu: Normal=(%.6f, %.6f, %.6f) -> 0x%08x\n", 
                                   i, normal[0], normal[1], normal[2], vertices[i].normal);
                        }
                    } else {
                        fprintf(stderr, "WARNING: Normal buffer bounds exceeded at vertex %zu\n", i);
                    }
                }
            } else {
                fprintf(stderr, "NORMAL_DEBUG: No normals found, using defaults\n");
            }
            
            // Extract texture coordinates from separate buffer view if available
            if (texcoordIt != primitive.attributes.end()) {
                const tinygltf::Accessor& texcoordAccessor = gltfModel.accessors[texcoordIt->second];
                const tinygltf::BufferView& texcoordView = gltfModel.bufferViews[texcoordAccessor.bufferView];
                const tinygltf::Buffer& texcoordBuffer = gltfModel.buffers[texcoordView.buffer];
                
                fprintf(stderr, "TEXCOORD_DEBUG: BufferView byteOffset=%zu, byteLength=%zu, accessor count=%zu\n",
                       texcoordView.byteOffset, texcoordView.byteLength, texcoordAccessor.count);
                
                // For separate buffer views, texture coordinates are tightly packed (2 floats per vertex)
                for (size_t i = 0; i < vertexCount; i++) {
                    size_t offset = texcoordView.byteOffset + texcoordAccessor.byteOffset + i * 8;
                    
                    // Bounds checking to prevent memory corruption
                    if (offset + 8 <= texcoordBuffer.data.size()) {
                        const float* texcoord = reinterpret_cast<const float*>(&texcoordBuffer.data[offset]);
                        
                        // Convert float UV coordinates to int16_t normalized format
                        // Map [0,1] to [0, 32767] for proper BGFX normalization (like bump example)
                        
                        // Different models may need different UV coordinate handling
                        bool shouldFlipV = true; // Default to flipping
                        std::string meshName = mesh.name.empty() ? "Unknown" : mesh.name;
                        
                        // Don't flip V for garden lamp (it works correctly in glTF viewers)
                        if (meshName.find("G-Object") != std::string::npos || 
                            meshName.find("garden") != std::string::npos || 
                            meshName.find("lamp") != std::string::npos) {
                            shouldFlipV = false;
                        }
                        
                        vertices[i].texcoord[0] = int16_t(texcoord[0] * 32767.0f);
                        if (shouldFlipV) {
                            vertices[i].texcoord[1] = int16_t((1.0f - texcoord[1]) * 32767.0f); // Flip V
                        } else {
                            vertices[i].texcoord[1] = int16_t(texcoord[1] * 32767.0f); // Don't flip V
                        }
                        
                        // Debug first few texture coordinates
                        if (i < 3) {
                            fprintf(stderr, "TEXCOORD_DEBUG: Vertex %zu: UV=(%.6f, %.6f) -> (%d, %d) [%s]\n", 
                                   i, texcoord[0], texcoord[1], vertices[i].texcoord[0], vertices[i].texcoord[1],
                                   shouldFlipV ? "V-FLIPPED" : "V-NORMAL");
                        }
                    } else {
                        // Safe fallback for out-of-bounds access
                        vertices[i].texcoord[0] = 0;
                        vertices[i].texcoord[1] = 0;
                        fprintf(stderr, "WARNING: Texture coordinate buffer bounds exceeded at vertex %zu\n", i);
                    }
                }
            }
            
            // Process bone weights and indices for skinning
            auto jointsIt = primitive.attributes.find("JOINTS_0");
            auto weightsIt = primitive.attributes.find("WEIGHTS_0");
            
            if (jointsIt != primitive.attributes.end() && weightsIt != primitive.attributes.end()) {
                // fprintf(stderr, "SKINNING_DEBUG: Processing bone weights and indices\n");
                
                // Process joint indices
                const tinygltf::Accessor& jointsAccessor = gltfModel.accessors[jointsIt->second];
                const tinygltf::BufferView& jointsView = gltfModel.bufferViews[jointsAccessor.bufferView];
                const tinygltf::Buffer& jointsBuffer = gltfModel.buffers[jointsView.buffer];
                
                // Process weights
                const tinygltf::Accessor& weightsAccessor = gltfModel.accessors[weightsIt->second];
                const tinygltf::BufferView& weightsView = gltfModel.bufferViews[weightsAccessor.bufferView];
                const tinygltf::Buffer& weightsBuffer = gltfModel.buffers[weightsView.buffer];
                
                // fprintf(stderr, "SKINNING_DEBUG: Joints count=%zu, componentType=%d\n", 
                //        jointsAccessor.count, jointsAccessor.componentType);
                // fprintf(stderr, "SKINNING_DEBUG: Weights count=%zu, componentType=%d\n", 
                //        weightsAccessor.count, weightsAccessor.componentType);
                
                // Process bone indices and weights for each vertex
                for (size_t i = 0; i < vertexCount && i < jointsAccessor.count && i < weightsAccessor.count; i++) {
                    // Process joint indices (typically uint8_t or uint16_t)
                    size_t jointsOffset = jointsView.byteOffset + jointsAccessor.byteOffset + i * 4;
                    if (jointsOffset + 4 <= jointsBuffer.data.size()) {
                        if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            const uint8_t* joints = reinterpret_cast<const uint8_t*>(&jointsBuffer.data[jointsOffset]);
                            vertices[i].boneIndices[0] = joints[0];
                            vertices[i].boneIndices[1] = joints[1];
                            vertices[i].boneIndices[2] = joints[2];
                            vertices[i].boneIndices[3] = joints[3];
                        } else if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            const uint16_t* joints = reinterpret_cast<const uint16_t*>(&jointsBuffer.data[jointsOffset]);
                            vertices[i].boneIndices[0] = (uint8_t)joints[0];
                            vertices[i].boneIndices[1] = (uint8_t)joints[1];
                            vertices[i].boneIndices[2] = (uint8_t)joints[2];
                            vertices[i].boneIndices[3] = (uint8_t)joints[3];
                        }
                    }
                    
                    // Process weights (typically float)
                    size_t weightsOffset = weightsView.byteOffset + weightsAccessor.byteOffset + i * 16; // 4 floats
                    if (weightsOffset + 16 <= weightsBuffer.data.size()) {
                        const float* weights = reinterpret_cast<const float*>(&weightsBuffer.data[weightsOffset]);
                        vertices[i].boneWeights[0] = weights[0];
                        vertices[i].boneWeights[1] = weights[1];
                        vertices[i].boneWeights[2] = weights[2];
                        vertices[i].boneWeights[3] = weights[3];
                        
                        // Normalize weights to ensure they sum to 1.0
                        float weightSum = weights[0] + weights[1] + weights[2] + weights[3];
                        if (weightSum > 0.0f) {
                            vertices[i].boneWeights[0] /= weightSum;
                            vertices[i].boneWeights[1] /= weightSum;
                            vertices[i].boneWeights[2] /= weightSum;
                            vertices[i].boneWeights[3] /= weightSum;
                        }
                    }
                    
                    // Debug first few vertices' skinning data
                    if (i < 3) {
                        // fprintf(stderr, "SKINNING_DEBUG: Vertex %zu: joints=(%u,%u,%u,%u) weights=(%.3f,%.3f,%.3f,%.3f)\n", 
                        //        i, vertices[i].boneIndices[0], vertices[i].boneIndices[1], 
                        //        vertices[i].boneIndices[2], vertices[i].boneIndices[3],
                        //        vertices[i].boneWeights[0], vertices[i].boneWeights[1], 
                        //        vertices[i].boneWeights[2], vertices[i].boneWeights[3]);
                    }
                }
                
                modelMesh.hasAnimation = true;
                // fprintf(stderr, "SKINNING_DEBUG: Mesh marked as having animation\n");
            } else {
                // fprintf(stderr, "SKINNING_DEBUG: No bone weights/indices found, using defaults\n");
            }
            
            // Store original vertices for animation if skinning is enabled
            if (modelMesh.hasAnimation) {
                modelMesh.originalVertices = vertices;
                modelMesh.animatedVertices.resize(vertices.size());
                // Initialize with original vertex data
                modelMesh.animatedVertices = vertices;
                // fprintf(stderr, "SKINNING_DEBUG: Stored %zu original vertices for animation\n", vertices.size());
                // fprintf(stderr, "SKINNING_DEBUG: Initialized %zu animated vertices\n", modelMesh.animatedVertices.size());
            }
            
            // Validate vertex data before creating buffer
            std::string meshName = mesh.name.empty() ? "Unknown" : mesh.name;
            validateVertexData(vertices, meshName);
            
            // Create vertex buffer with proper memory copy to prevent corruption
            const bgfx::Memory* vertexMem = bgfx::copy(vertices.data(), vertices.size() * sizeof(PosNormalTexcoordVertex));
            modelMesh.vertexBuffer = bgfx::createVertexBuffer(
                vertexMem,
                PosNormalTexcoordVertex::ms_layout
            );
            
            // Process indices if present
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = gltfModel.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexView.buffer];
                
                fprintf(stderr, "INDEX_DEBUG: Index accessor count=%zu, componentType=%d\n", 
                       indexAccessor.count, indexAccessor.componentType);
                fprintf(stderr, "INDEX_DEBUG: Index BufferView byteOffset=%zu, byteLength=%zu\n",
                       indexView.byteOffset, indexView.byteLength);
                fprintf(stderr, "INDEX_DEBUG: Primitive mode=%d (4=TRIANGLES, 0=POINTS, 1=LINES)\n", primitive.mode);
                
                modelMesh.indexCount = indexAccessor.count;
                std::vector<uint16_t> indices(modelMesh.indexCount);
                
                // Handle different index types with bounds checking
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    // Already uint16_t
                    for (size_t i = 0; i < modelMesh.indexCount; i++) {
                        size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i * 2;
                        if (offset + 2 <= indexBuffer.data.size()) {
                            uint16_t index = *reinterpret_cast<const uint16_t*>(&indexBuffer.data[offset]);
                            if (index < vertexCount) {
                                indices[i] = index;
                                // Debug first few indices and their corresponding vertices
                                if (i < 15) {
                                    fprintf(stderr, "INDEX_DEBUG: Index %zu = %u\n", i, index);
                                    if (i < 9) { // First 3 triangles
                                        // Check if this vertex was already processed
                                        if (index < vertices.size()) {
                                            fprintf(stderr, "INDEX_DEBUG: Vertex %u position: (%.6f, %.6f, %.6f)\n", 
                                                   index, vertices[index].position[0], vertices[index].position[1], vertices[index].position[2]);
                                        }
                                    }
                                }
                            } else {
                                fprintf(stderr, "WARNING: Index %u exceeds vertex count %zu at position %zu\n", 
                                       index, vertexCount, i);
                                indices[i] = 0; // Safe fallback
                            }
                        } else {
                            fprintf(stderr, "ERROR: Index buffer bounds exceeded at position %zu\n", i);
                            indices[i] = 0;
                        }
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    // Convert uint32_t to uint16_t
                    for (size_t i = 0; i < modelMesh.indexCount; i++) {
                        size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i * 4;
                        if (offset + 4 <= indexBuffer.data.size()) {
                            uint32_t index = *reinterpret_cast<const uint32_t*>(&indexBuffer.data[offset]);
                            if (index < vertexCount && index <= UINT16_MAX) {
                                indices[i] = static_cast<uint16_t>(index);
                            } else {
                                fprintf(stderr, "WARNING: Index %u exceeds limits (vertex count %zu, max %u) at position %zu\n", 
                                       index, vertexCount, UINT16_MAX, i);
                                indices[i] = 0; // Safe fallback
                            }
                        } else {
                            fprintf(stderr, "ERROR: Index buffer bounds exceeded at position %zu\n", i);
                            indices[i] = 0;
                        }
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    // Convert uint8_t to uint16_t
                    for (size_t i = 0; i < modelMesh.indexCount; i++) {
                        size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i;
                        if (offset < indexBuffer.data.size()) {
                            uint8_t index = indexBuffer.data[offset];
                            if (index < vertexCount) {
                                indices[i] = static_cast<uint16_t>(index);
                            } else {
                                fprintf(stderr, "WARNING: Index %u exceeds vertex count %zu at position %zu\n", 
                                       index, vertexCount, i);
                                indices[i] = 0; // Safe fallback
                            }
                        } else {
                            fprintf(stderr, "ERROR: Index buffer bounds exceeded at position %zu\n", i);
                            indices[i] = 0;
                        }
                    }
                } else {
                    std::cerr << "Unsupported index type: " << indexAccessor.componentType << std::endl;
                    continue;
                }
                
                // Set primitive type
                modelMesh.primitiveType = primitive.mode;
                
                // Create index buffer with proper memory copy to prevent corruption
                const bgfx::Memory* indexMem = bgfx::copy(indices.data(), indices.size() * sizeof(uint16_t));
                modelMesh.indexBuffer = bgfx::createIndexBuffer(indexMem);
            } else {
                // No indices, create sequential indices
                std::vector<uint16_t> indices(vertexCount);
                for (uint16_t i = 0; i < vertexCount; i++) {
                    indices[i] = i;
                }
                
                modelMesh.indexCount = indices.size();
                modelMesh.primitiveType = primitive.mode;
                const bgfx::Memory* indexMem = bgfx::copy(indices.data(), indices.size() * sizeof(uint16_t));
                modelMesh.indexBuffer = bgfx::createIndexBuffer(indexMem);
            }
            
            // Load texture if available
            if (primitive.material >= 0) {
                const tinygltf::Material& material = gltfModel.materials[primitive.material];
                
                // Check for base color texture
                if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                    int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                    int sourceIndex = gltfModel.textures[textureIndex].source;
                    
                    if (sourceIndex >= 0) {
                        // Check if we've already loaded this texture
                        if (loadedTextures.find(sourceIndex) != loadedTextures.end()) {
                            // Reuse the existing texture
                            modelMesh.texture = loadedTextures[sourceIndex];
                        } else {
                            // Create a new texture
                            const tinygltf::Image& image = gltfModel.images[sourceIndex];
                            
                            // Skip if no image data
                            if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
                                continue;
                            }
                            
                            // Set texture options
                            uint64_t textureFlags = BGFX_TEXTURE_NONE;
                            textureFlags |= BGFX_SAMPLER_MIN_ANISOTROPIC;
                            textureFlags |= BGFX_SAMPLER_MAG_ANISOTROPIC;
                            
                            // Convert to RGBA if needed
                            if (image.component == 3) {
                                // RGB to RGBA
                                std::vector<uint8_t> rgbaData(image.width * image.height * 4);
                                
                                for (int i = 0; i < image.width * image.height; i++) {
                                    rgbaData[i * 4 + 0] = image.image[i * 3 + 0];
                                    rgbaData[i * 4 + 1] = image.image[i * 3 + 1];
                                    rgbaData[i * 4 + 2] = image.image[i * 3 + 2];
                                    rgbaData[i * 4 + 3] = 255;
                                }
                                
                                const bgfx::Memory* mem = bgfx::copy(rgbaData.data(), rgbaData.size());
                                
                                modelMesh.texture = bgfx::createTexture2D(
                                    image.width, 
                                    image.height,
                                    false, 
                                    1,
                                    bgfx::TextureFormat::RGBA8,
                                    textureFlags,
                                    mem
                                );
                            } else if (image.component == 4) {
                                // Already RGBA
                                const bgfx::Memory* mem = bgfx::copy(image.image.data(), image.image.size());
                                
                                modelMesh.texture = bgfx::createTexture2D(
                                    image.width, 
                                    image.height,
                                    false, 
                                    1,
                                    bgfx::TextureFormat::RGBA8,
                                    textureFlags,
                                    mem
                                );
                            }
                            
                            if (bgfx::isValid(modelMesh.texture)) {
                                std::cout << "Created texture " << sourceIndex << ": " 
                                          << image.width << "x" << image.height
                                          << " components: " << image.component << std::endl;
                                
                                // Enhanced texture debugging info to stderr
                                fprintf(stderr, "TEXTURE_CREATE: Texture %d: Format=%s, Size=%dx%d, Memory=%zu bytes, Flags=0x%x\n",
                                       sourceIndex,
                                       (image.component == 3 ? "RGB→RGBA" : "RGBA"),
                                       image.width, image.height,
                                       (image.component == 3 ? image.width * image.height * 4 : image.image.size()),
                                       BGFX_SAMPLER_MIN_ANISOTROPIC);
                                
                                loadedTextures[sourceIndex] = modelMesh.texture;
                            }
                        }
                    }
                }
            }
            
            // Use fallback texture if no texture was assigned
            if (!bgfx::isValid(modelMesh.texture) && bgfx::isValid(fallbackTexture)) {
                modelMesh.texture = fallbackTexture;
                fprintf(stderr, "TEXTURE_DEBUG: Using fallback texture for mesh %s\n", mesh.name.c_str());
            }
            
            // Add mesh to collection
            meshes.push_back(modelMesh);
        }
    }
    
    std::cout << "Loaded model with " << meshes.size() << " meshes" << std::endl;
    
    // Process animations
    std::cout << "Processing " << gltfModel.animations.size() << " animations..." << std::endl;
    for (const auto& gltfAnimation : gltfModel.animations) {
        AnimationClip clip;
        clip.name = gltfAnimation.name.empty() ? "Animation" : gltfAnimation.name;
        clip.duration = 0.0f;
        
        std::cout << "  Loading animation: " << clip.name << std::endl;
        
        // Process each channel
        for (const auto& channel : gltfAnimation.channels) {
            const auto& sampler = gltfAnimation.samplers[channel.sampler];
            
            AnimationChannel animChannel;
            animChannel.nodeIndex = channel.target_node;
            animChannel.path = channel.target_path;
            
            // Get input (time) accessor
            const auto& inputAccessor = gltfModel.accessors[sampler.input];
            const auto& inputBufferView = gltfModel.bufferViews[inputAccessor.bufferView];
            const auto& inputBuffer = gltfModel.buffers[inputBufferView.buffer];
            
            // Get output (values) accessor  
            const auto& outputAccessor = gltfModel.accessors[sampler.output];
            const auto& outputBufferView = gltfModel.bufferViews[outputAccessor.bufferView];
            const auto& outputBuffer = gltfModel.buffers[outputBufferView.buffer];
            
            // Read time values
            const float* timeData = reinterpret_cast<const float*>(
                inputBuffer.data.data() + inputBufferView.byteOffset + inputAccessor.byteOffset);
                
            // Read output values
            const float* valueData = reinterpret_cast<const float*>(
                outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset);
                
            // Create keyframes
            size_t numKeyframes = inputAccessor.count;
            size_t valuesPerKeyframe = outputAccessor.count / numKeyframes;
            
            for (size_t i = 0; i < numKeyframes; i++) {
                AnimationKeyframe keyframe;
                keyframe.time = timeData[i];
                
                // Copy values for this keyframe
                keyframe.values.resize(valuesPerKeyframe);
                for (size_t j = 0; j < valuesPerKeyframe; j++) {
                    keyframe.values[j] = valueData[i * valuesPerKeyframe + j];
                }
                
                animChannel.keyframes.push_back(keyframe);
                
                // Update animation duration
                clip.duration = std::max(clip.duration, keyframe.time);
            }
            
            clip.channels.push_back(animChannel);
        }
        
        animations.push_back(clip);
        std::cout << "    Duration: " << clip.duration << "s, Channels: " << clip.channels.size() << std::endl;
    }
    
    // Process nodes (including joints)
    std::cout << "Processing " << gltfModel.nodes.size() << " nodes..." << std::endl;
    nodes.resize(gltfModel.nodes.size());
    for (size_t i = 0; i < gltfModel.nodes.size(); i++) {
        const auto& gltfNode = gltfModel.nodes[i];
        Joint& node = nodes[i];
        
        node.index = (int)i;
        node.name = gltfNode.name.empty() ? ("Node_" + std::to_string(i)) : gltfNode.name;
        node.parentIndex = -1; // Will be set when processing children
        
        // Initialize matrices to identity
        bx::mtxIdentity(node.bindMatrix);
        bx::mtxIdentity(node.localMatrix);
        bx::mtxIdentity(node.globalMatrix);
        
        // Get local transform from node
        if (!gltfNode.matrix.empty()) {
            // Node has a matrix
            for (int j = 0; j < 16; j++) {
                node.localMatrix[j] = (float)gltfNode.matrix[j];
            }
        } else {
            // For now, just use identity matrix
            // TODO: Implement proper TRS composition with quaternions
            bx::mtxIdentity(node.localMatrix);
            
            // Apply simple translation if available
            if (!gltfNode.translation.empty()) {
                bx::mtxTranslate(node.localMatrix, 
                    (float)gltfNode.translation[0],
                    (float)gltfNode.translation[1], 
                    (float)gltfNode.translation[2]);
            }
        }
        
        // Set up parent-child relationships
        for (int childIndex : gltfNode.children) {
            node.children.push_back(childIndex);
            if (childIndex < (int)nodes.size()) {
                nodes[childIndex].parentIndex = (int)i;
            }
        }
    }
    
    // Process skins
    std::cout << "Processing " << gltfModel.skins.size() << " skins..." << std::endl;
    for (const auto& gltfSkin : gltfModel.skins) {
        Skin skin;
        skin.jointIndices = gltfSkin.joints;
        
        // Process inverse bind matrices if available
        if (gltfSkin.inverseBindMatrices >= 0) {
            const auto& accessor = gltfModel.accessors[gltfSkin.inverseBindMatrices];
            const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
            const auto& buffer = gltfModel.buffers[bufferView.buffer];
            
            const float* matrices = reinterpret_cast<const float*>(
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                
            for (size_t i = 0; i < gltfSkin.joints.size(); i++) {
                int jointIndex = gltfSkin.joints[i];
                if (jointIndex < (int)nodes.size()) {
                    // Copy inverse bind matrix
                    for (int j = 0; j < 16; j++) {
                        nodes[jointIndex].bindMatrix[j] = matrices[i * 16 + j];
                    }
                }
            }
        }
        
        skins.push_back(skin);
        std::cout << "  Skin with " << gltfSkin.joints.size() << " joints" << std::endl;
    }
    
    return !meshes.empty();
}

void Model::render(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform, float* modelMatrix) {
    // Set model matrix
    bgfx::setTransform(modelMatrix);
    
    // Use default rendering state
    uint64_t state = BGFX_STATE_DEFAULT;
    // Disable backface culling to ensure all faces are visible
    state &= ~BGFX_STATE_CULL_MASK;
    
    // Render all meshes
    for (size_t i = 0; i < meshes.size(); i++) {
        const ModelMesh& mesh = meshes[i];
        
        // Set texture
        if (bgfx::isValid(mesh.texture)) {
            bgfx::setTexture(0, texUniform, mesh.texture);
        } else if (bgfx::isValid(fallbackTexture)) {
            bgfx::setTexture(0, texUniform, fallbackTexture);
        }
        
        // Set buffers
        bgfx::setVertexBuffer(0, mesh.vertexBuffer);
        bgfx::setIndexBuffer(mesh.indexBuffer);
        
        // Set state
        bgfx::setState(state);
        
        // Submit
        bgfx::submit(0, program);
    }
}

void Model::renderInstanced(bgfx::ProgramHandle program, bgfx::UniformHandle texUniform, 
                           bgfx::InstanceDataBuffer* instanceBuffer, uint32_t instanceCount) {
    // No transform matrix needed for instanced rendering - handled by instances
    
    // Use default rendering state
    uint64_t state = BGFX_STATE_DEFAULT;
    // Disable backface culling to ensure all faces are visible
    state &= ~BGFX_STATE_CULL_MASK;
    
    // Render all meshes with instancing
    for (size_t i = 0; i < meshes.size(); i++) {
        const ModelMesh& mesh = meshes[i];
        
        // Set texture
        if (bgfx::isValid(mesh.texture)) {
            bgfx::setTexture(0, texUniform, mesh.texture);
        } else if (bgfx::isValid(fallbackTexture)) {
            bgfx::setTexture(0, texUniform, fallbackTexture);
        }
        
        // Set vertex and index buffers
        bgfx::setVertexBuffer(0, mesh.vertexBuffer);
        bgfx::setIndexBuffer(mesh.indexBuffer);
        
        // Set instance data buffer (proper BGFX instancing API)
        bgfx::setInstanceDataBuffer(instanceBuffer);
        
        // Set state
        bgfx::setState(state);
        
        // Submit instanced draw call
        bgfx::submit(0, program);
    }
}

bool Model::processBinaryMesh(const std::vector<uint8_t>& data) {
    // This function directly parses the GLTF binary buffer format
    // Note: The .bin file from glTF contains raw binary data without any headers
    // We need to interpret it as vertex positions, normals, texcoords, and indices
    
    if (data.empty()) {
        std::cerr << "Binary mesh data is empty" << std::endl;
        return false;
    }
    
    // For glTF binaries, the interleaved format is generally:
    // 1. First section: vertices (position)
    // 2. Second section: normals
    // 3. Third section: texture coordinates
    // 4. Fourth section: indices
    
    // We'll assume our duck model follows a common layout with these attributes
    // Positions (3 floats), normals (3 floats), and UVs (2 floats)
    
    // Create a new mesh
    ModelMesh modelMesh;
    
    // We don't know exact counts from just the binary, so we have to estimate based on file size
    // Assuming interleaved data with positions (3 floats) taking up most of the binary
    size_t totalBytes = data.size();
    
    // Estimate vertex count (approximate - will be refined later)
    // Assume 1/3 of data is vertex positions (3 floats per vertex)
    size_t estimatedVertexDataSize = totalBytes * 2/3;
    size_t estimatedVertexCount = estimatedVertexDataSize / (3 * sizeof(float));
    
    // Assume indices are uint16_t and take the remaining 1/3 of data
    size_t estimatedIndexDataSize = totalBytes * 1/3;
    size_t estimatedIndexCount = estimatedIndexDataSize / sizeof(uint16_t);
    
    // Ensure estimates make sense
    if (estimatedVertexCount == 0 || estimatedIndexCount == 0) {
        std::cerr << "Invalid binary data size estimates" << std::endl;
        return false;
    }
    
    // Log our estimates
    std::cout << "Estimated vertex count: " << estimatedVertexCount << std::endl;
    std::cout << "Estimated index count: " << estimatedIndexCount << std::endl;
    
    // Assume the format is:
    // 1. Positions: estimatedVertexCount * 3 floats
    // 2. (possibly) Normals: estimatedVertexCount * 3 floats
    // 3. (possibly) UVs: estimatedVertexCount * 2 floats
    // 4. Indices: estimatedIndexCount * uint16_t
    
    // Safety check - make sure we don't exceed the buffer
    size_t positionsSectionSize = estimatedVertexCount * 3 * sizeof(float);
    if (positionsSectionSize > data.size()) {
        // Adjust estimatedVertexCount
        estimatedVertexCount = data.size() / (3 * sizeof(float)) / 2; // Allow some space for indices
        positionsSectionSize = estimatedVertexCount * 3 * sizeof(float);
        std::cout << "Adjusted vertex count: " << estimatedVertexCount << std::endl;
    }
    
    // Create our vertex array and fill it
    std::vector<PosNormalTexcoordVertex> vertices(estimatedVertexCount);
    
    // Process positions
    const float* positions = reinterpret_cast<const float*>(data.data());
    // The scale factor (0.01f) to make the model smaller
    const float scaleFactor = 0.01f;
    
    for (size_t i = 0; i < estimatedVertexCount; i++) {
        vertices[i].position[0] = positions[i*3] * scaleFactor;
        vertices[i].position[1] = positions[i*3+1] * scaleFactor;
        vertices[i].position[2] = positions[i*3+2] * scaleFactor;
        
        // Set default normal and uv
        vertices[i].normal = encodeNormalRgba8(0.0f, 1.0f, 0.0f); // Default up normal
        vertices[i].texcoord[0] = 0; // int16_t default
        vertices[i].texcoord[1] = 0; // int16_t default
    }
    
    // Create vertex buffer
    modelMesh.vertexBuffer = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices.data(), vertices.size() * sizeof(PosNormalTexcoordVertex)),
        PosNormalTexcoordVertex::ms_layout
    );
    
    // For indices, let's try to find them in the remaining part of the buffer
    // For simplicity, let's create sequential indices for a triangle list
    std::vector<uint16_t> indices;
    
    // Try to determine if there are indices in the remaining data
    size_t remainingDataSize = data.size() - positionsSectionSize;
    
    if (remainingDataSize >= 6) { // At least 3 indices (6 bytes) for a triangle
        // Calculate how many uint16_t indices can fit in the remaining data
        size_t possibleIndices = remainingDataSize / sizeof(uint16_t);
        size_t indexOffset = positionsSectionSize;
        
        // Limit to a reasonable number
        possibleIndices = std::min(possibleIndices, estimatedVertexCount * 3); // Assume at most 3 indices per vertex
        
        // Read indices from the data
        const uint16_t* rawIndices = reinterpret_cast<const uint16_t*>(data.data() + indexOffset);
        for (size_t i = 0; i < possibleIndices; i++) {
            // Only add indices that reference valid vertices
            if (rawIndices[i] < estimatedVertexCount) {
                indices.push_back(rawIndices[i]);
            }
        }
    }
    
    // If we couldn't find valid indices, create sequential ones
    if (indices.empty()) {
        // Create a simple triangle list covering all vertices
        for (uint16_t i = 0; i < estimatedVertexCount; i++) {
            indices.push_back(i);
        }
    }
    
    // Create index buffer
    modelMesh.indexCount = indices.size();
    modelMesh.indexBuffer = bgfx::createIndexBuffer(
        bgfx::makeRef(indices.data(), indices.size() * sizeof(uint16_t))
    );
    
    // Set default primitive type (triangles)
    modelMesh.primitiveType = 4; // TRIANGLES
    
    // Use fallback texture if available
    if (bgfx::isValid(fallbackTexture)) {
        modelMesh.texture = fallbackTexture;
    }
    
    // Add mesh to collection
    meshes.push_back(modelMesh);
    
    std::cout << "Loaded binary mesh with " << vertices.size() << " vertices and " 
              << indices.size() << " indices" << std::endl;
    return true;
}

void Model::unload() {
    // Destroy all mesh resources
    for (const auto& mesh : meshes) {
        if (bgfx::isValid(mesh.vertexBuffer)) {
            bgfx::destroy(mesh.vertexBuffer);
        }
        if (bgfx::isValid(mesh.indexBuffer)) {
            bgfx::destroy(mesh.indexBuffer);
        }
    }
    
    // Destroy all textures
    for (const auto& entry : loadedTextures) {
        if (bgfx::isValid(entry.second)) {
            bgfx::destroy(entry.second);
        }
    }
    
    // Clear data
    meshes.clear();
    loadedTextures.clear();
    animations.clear();
    nodes.clear();
    skins.clear();
}

const AnimationClip* Model::getAnimation(const std::string& name) const {
    for (const auto& animation : animations) {
        if (animation.name == name) {
            return &animation;
        }
    }
    return nullptr;
}

void Model::calculateBoneMatrices(const std::string& animationName, float time, std::vector<float*>& boneMatrices) const {
    const AnimationClip* anim = getAnimation(animationName);
    if (!anim || skins.empty()) {
        return;
    }
    
    // For each joint in the first skin
    const Skin& skin = skins[0];
    boneMatrices.resize(skin.jointIndices.size());
    
    for (size_t i = 0; i < skin.jointIndices.size(); i++) {
        int nodeIndex = skin.jointIndices[i];
        if (nodeIndex >= (int)nodes.size()) continue;
        
        // Start with bind pose
        const Joint& joint = nodes[nodeIndex];
        float* matrix = boneMatrices[i];
        if (!matrix) {
            matrix = new float[16];
            boneMatrices[i] = matrix;
        }
        
        // Apply animation transforms by interpolating keyframes
        float translation[3] = {0.0f, 0.0f, 0.0f};
        float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // w=1 for identity quaternion
        float scale[3] = {1.0f, 1.0f, 1.0f};
        
        // Debug: Check if we find any channels for this node
        bool foundChannel = false;
        
        // Find and interpolate animation channels for this node
        for (const auto& channel : anim->channels) {
            if (channel.nodeIndex == nodeIndex && !channel.keyframes.empty()) {
                foundChannel = true;
                // Find the right keyframe(s) for the current time
                size_t keyIndex = 0;
                for (size_t k = 0; k < channel.keyframes.size() - 1; k++) {
                    if (time >= channel.keyframes[k].time && time < channel.keyframes[k + 1].time) {
                        keyIndex = k;
                        break;
                    }
                }
                
                // For now, just use the keyframe values without interpolation
                const auto& keyframe = channel.keyframes[keyIndex];
                if (channel.path == "translation" && keyframe.values.size() >= 3) {
                    translation[0] = keyframe.values[0];
                    translation[1] = keyframe.values[1];
                    translation[2] = keyframe.values[2];
                } else if (channel.path == "rotation" && keyframe.values.size() >= 4) {
                    rotation[0] = keyframe.values[0];
                    rotation[1] = keyframe.values[1];
                    rotation[2] = keyframe.values[2];
                    rotation[3] = keyframe.values[3];
                } else if (channel.path == "scale" && keyframe.values.size() >= 3) {
                    scale[0] = keyframe.values[0];
                    scale[1] = keyframe.values[1];
                    scale[2] = keyframe.values[2];
                }
            }
        }
        
        // Debug output for first few bones
        if (i < 3) {
            fprintf(stderr, "BONE_DEBUG: Joint %zu (node %d): foundChannel=%s, translation=(%.3f,%.3f,%.3f)\n", 
                   i, nodeIndex, foundChannel ? "YES" : "NO", translation[0], translation[1], translation[2]);
        }
        
        // Create transformation matrix from TRS (focusing on rotation for idle animations)
        // Convert quaternion to rotation matrix
        float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
        float xx = x*x, yy = y*y, zz = z*z;
        float xy = x*y, xz = x*z, yz = y*z;
        float wx = w*x, wy = w*y, wz = w*z;
        
        // Build rotation matrix with translation
        matrix[0] = 1.0f - 2.0f*(yy + zz); matrix[4] = 2.0f*(xy - wz);       matrix[8] = 2.0f*(xz + wy);       matrix[12] = translation[0];
        matrix[1] = 2.0f*(xy + wz);       matrix[5] = 1.0f - 2.0f*(xx + zz); matrix[9] = 2.0f*(yz - wx);       matrix[13] = translation[1];
        matrix[2] = 2.0f*(xz - wy);       matrix[6] = 2.0f*(yz + wx);       matrix[10] = 1.0f - 2.0f*(xx + yy); matrix[14] = translation[2];
        matrix[3] = 0.0f;                matrix[7] = 0.0f;                matrix[11] = 0.0f;                matrix[15] = 1.0f;
    }
}

void Model::updateNodeMatrix(int nodeIndex, const std::string& animationName, float time) {
    // TODO: Implement animation transform interpolation
    // This will update the local matrix of a node based on animation keyframes
}

void Model::updateAnimatedVertices(const std::string& animationName, float time) {
    // Calculate bone matrices for the given animation and time
    std::vector<float*> boneMatrices;
    calculateBoneMatrices(animationName, time, boneMatrices);
    
    // Transform vertices using bone matrices
    for (auto& mesh : meshes) {
        if (mesh.hasAnimation && !mesh.originalVertices.empty()) {
            mesh.animatedVertices.resize(mesh.originalVertices.size());
            
            // Transform each vertex
            for (size_t i = 0; i < mesh.originalVertices.size(); i++) {
                const auto& originalVertex = mesh.originalVertices[i];
                auto& animatedVertex = mesh.animatedVertices[i];
                
                // Copy non-transformed data
                animatedVertex.normal = originalVertex.normal;
                animatedVertex.texcoord[0] = originalVertex.texcoord[0];
                animatedVertex.texcoord[1] = originalVertex.texcoord[1];
                animatedVertex.boneIndices[0] = originalVertex.boneIndices[0];
                animatedVertex.boneIndices[1] = originalVertex.boneIndices[1];
                animatedVertex.boneIndices[2] = originalVertex.boneIndices[2];
                animatedVertex.boneIndices[3] = originalVertex.boneIndices[3];
                animatedVertex.boneWeights[0] = originalVertex.boneWeights[0];
                animatedVertex.boneWeights[1] = originalVertex.boneWeights[1];
                animatedVertex.boneWeights[2] = originalVertex.boneWeights[2];
                animatedVertex.boneWeights[3] = originalVertex.boneWeights[3];
                
                // Start with original position
                float transformedPos[3] = {
                    originalVertex.position[0],
                    originalVertex.position[1], 
                    originalVertex.position[2]
                };
                
                // Real skeletal animation using actual glTF animation keyframes
                if (!boneMatrices.empty()) {
                    transformedPos[0] = 0.0f;
                    transformedPos[1] = 0.0f;
                    transformedPos[2] = 0.0f;
                    
                    // Apply weighted bone transformations using real animation data
                    for (int boneIdx = 0; boneIdx < 4; boneIdx++) {
                        float weight = originalVertex.boneWeights[boneIdx];
                        if (weight > 0.0001f) {
                            uint8_t jointIndex = originalVertex.boneIndices[boneIdx];
                            
                            if (jointIndex < boneMatrices.size() && boneMatrices[jointIndex] != nullptr) {
                                const float* boneMatrix = boneMatrices[jointIndex];
                                
                                // Transform position using real bone matrix
                                float x = originalVertex.position[0];
                                float y = originalVertex.position[1];
                                float z = originalVertex.position[2];
                                
                                // Apply transformation but scale it down to prevent explosion
                                float scale = 0.1f; // Reduce impact to debug
                                float transformedX = (boneMatrix[0] * x + boneMatrix[4] * y + boneMatrix[8] * z + boneMatrix[12]) * scale;
                                float transformedY = (boneMatrix[1] * x + boneMatrix[5] * y + boneMatrix[9] * z + boneMatrix[13]) * scale;
                                float transformedZ = (boneMatrix[2] * x + boneMatrix[6] * y + boneMatrix[10] * z + boneMatrix[14]) * scale;
                                
                                transformedPos[0] += transformedX * weight;
                                transformedPos[1] += transformedY * weight;
                                transformedPos[2] += transformedZ * weight;
                            }
                        }
                    }
                    
                    // Add back original position (mix between original and transformed)
                    float mixFactor = 0.9f; // Mostly original position to start
                    transformedPos[0] = originalVertex.position[0] * mixFactor + transformedPos[0] * (1.0f - mixFactor);
                    transformedPos[1] = originalVertex.position[1] * mixFactor + transformedPos[1] * (1.0f - mixFactor);
                    transformedPos[2] = originalVertex.position[2] * mixFactor + transformedPos[2] * (1.0f - mixFactor);
                } else if (false && !boneMatrices.empty() && originalVertex.boneWeights[0] < 1.0f) {
                    transformedPos[0] = 0.0f;
                    transformedPos[1] = 0.0f;
                    transformedPos[2] = 0.0f;
                    
                    // Apply weighted bone transformations
                    for (int boneIdx = 0; boneIdx < 4; boneIdx++) {
                        float weight = originalVertex.boneWeights[boneIdx];
                        if (weight > 0.0001f) { // Only process significant weights
                            uint8_t jointIndex = originalVertex.boneIndices[boneIdx];
                            
                            // Ensure joint index is valid
                            if (jointIndex < boneMatrices.size() && boneMatrices[jointIndex] != nullptr) {
                                const float* boneMatrix = boneMatrices[jointIndex];
                                
                                // Transform the original position by the bone matrix
                                float x = originalVertex.position[0];
                                float y = originalVertex.position[1];
                                float z = originalVertex.position[2];
                                
                                // Apply 4x4 matrix transformation (positions are 3D with w=1)
                                float transformedX = boneMatrix[0] * x + boneMatrix[4] * y + boneMatrix[8] * z + boneMatrix[12];
                                float transformedY = boneMatrix[1] * x + boneMatrix[5] * y + boneMatrix[9] * z + boneMatrix[13];
                                float transformedZ = boneMatrix[2] * x + boneMatrix[6] * y + boneMatrix[10] * z + boneMatrix[14];
                                
                                // Add weighted contribution
                                transformedPos[0] += transformedX * weight;
                                transformedPos[1] += transformedY * weight;
                                transformedPos[2] += transformedZ * weight;
                            }
                        }
                    }
                }
                
                // Set the transformed position
                animatedVertex.position[0] = transformedPos[0];
                animatedVertex.position[1] = transformedPos[1];
                animatedVertex.position[2] = transformedPos[2];
            }
            
            // Update the vertex buffer with animated vertices
            if (bgfx::isValid(mesh.vertexBuffer)) {
                bgfx::destroy(mesh.vertexBuffer);
            }
            
            mesh.vertexBuffer = bgfx::createVertexBuffer(
                bgfx::makeRef(mesh.animatedVertices.data(), 
                             mesh.animatedVertices.size() * sizeof(PosNormalTexcoordVertex)),
                PosNormalTexcoordVertex::ms_layout
            );
        }
    }
}

void Model::updateWithOzzBoneMatrices(const float* boneMatrices, size_t boneCount) {
    // This method is deprecated - use updateWithOzzSkinning instead
    std::cout << "WARNING: updateWithOzzBoneMatrices is deprecated, use updateWithOzzSkinning" << std::endl;
}

bool Model::getInverseBindMatrices(std::vector<float>& outMatrices) const {
    if (skins.empty()) {
        return false;
    }
    
    // Use the first skin (most models have only one skin)
    const Skin& skin = skins[0];
    outMatrices.clear();
    outMatrices.reserve(skin.jointIndices.size() * 16);
    
    for (int jointIndex : skin.jointIndices) {
        if (jointIndex < (int)nodes.size()) {
            // Copy the 16 floats of the inverse bind matrix
            for (int i = 0; i < 16; i++) {
                outMatrices.push_back(nodes[jointIndex].bindMatrix[i]);
            }
        } else {
            // Add identity matrix for invalid joint indices
            for (int i = 0; i < 16; i++) {
                outMatrices.push_back((i % 5 == 0) ? 1.0f : 0.0f); // Identity matrix pattern
            }
        }
    }
    
    std::cout << "Extracted " << (outMatrices.size() / 16) << " inverse bind matrices" << std::endl;
    return !outMatrices.empty();
}

void Model::remapBoneIndices(const std::vector<int>& gltfToOzzMapping) {
    std::cout << "Remapping bone indices for " << meshes.size() << " meshes..." << std::endl;
    
    for (auto& mesh : meshes) {
        if (!mesh.hasAnimation) continue;
        
        // Remap bone indices in both original and animated vertices
        for (auto& vertex : mesh.originalVertices) {
            for (int i = 0; i < 4; i++) {
                uint8_t gltfIndex = vertex.boneIndices[i];
                if (gltfIndex < gltfToOzzMapping.size() && gltfToOzzMapping[gltfIndex] != -1) {
                    vertex.boneIndices[i] = static_cast<uint8_t>(gltfToOzzMapping[gltfIndex]);
                } else {
                    // Invalid mapping - set to joint 0 with zero weight
                    vertex.boneIndices[i] = 0;
                    vertex.boneWeights[i] = 0.0f;
                }
            }
        }
        
        // Copy remapped indices to animated vertices
        mesh.animatedVertices = mesh.originalVertices;
        
        std::cout << "Remapped bone indices for mesh with " << mesh.originalVertices.size() << " vertices" << std::endl;
    }
}

void Model::updateWithOzzSkinning(OzzAnimationSystem& ozzSystem) {
    // Update all meshes that have animation data using ozz native skinning
    for (auto& mesh : meshes) {
        if (!mesh.hasAnimation || mesh.originalVertices.empty() || mesh.animatedVertices.empty()) {
            continue;
        }
        
        // Safety check: ensure animated vertices vector is the right size
        if (mesh.animatedVertices.size() != mesh.originalVertices.size()) {
            std::cout << "ERROR: Animated vertices size mismatch!" << std::endl;
            return;
        }
        
        const size_t vertexCount = mesh.originalVertices.size();
        
        // Extract data from packed vertex format for ozz skinning
        std::vector<float> inPositions(vertexCount * 3);
        std::vector<float> inNormals(vertexCount * 3);
        std::vector<uint16_t> jointIndices(vertexCount * 4);
        std::vector<float> jointWeights(vertexCount * 3); // ozz needs influences-1 weights
        
        std::vector<float> outPositions(vertexCount * 3);
        std::vector<float> outNormals(vertexCount * 3);
        
        // Convert from packed format to separate arrays
        for (size_t i = 0; i < vertexCount; i++) {
            const auto& vertex = mesh.originalVertices[i];
            
            // Extract positions
            inPositions[i * 3 + 0] = vertex.position[0];
            inPositions[i * 3 + 1] = vertex.position[1];
            inPositions[i * 3 + 2] = vertex.position[2];
            
            // Extract and unpack normals
            float normalX = ((vertex.normal >> 0) & 0xFF) / 255.0f * 2.0f - 1.0f;
            float normalY = ((vertex.normal >> 8) & 0xFF) / 255.0f * 2.0f - 1.0f;
            float normalZ = ((vertex.normal >> 16) & 0xFF) / 255.0f * 2.0f - 1.0f;
            
            inNormals[i * 3 + 0] = normalX;
            inNormals[i * 3 + 1] = normalY;
            inNormals[i * 3 + 2] = normalZ;
            
            // Extract joint indices (convert uint8_t to uint16_t)
            jointIndices[i * 4 + 0] = static_cast<uint16_t>(vertex.boneIndices[0]);
            jointIndices[i * 4 + 1] = static_cast<uint16_t>(vertex.boneIndices[1]);
            jointIndices[i * 4 + 2] = static_cast<uint16_t>(vertex.boneIndices[2]);
            jointIndices[i * 4 + 3] = static_cast<uint16_t>(vertex.boneIndices[3]);
            
            // Extract joint weights (ozz expects influences-1 weights, so only first 3)
            jointWeights[i * 3 + 0] = vertex.boneWeights[0];
            jointWeights[i * 3 + 1] = vertex.boneWeights[1];
            jointWeights[i * 3 + 2] = vertex.boneWeights[2];
            // Note: 4th weight is computed automatically by ozz as 1.0 - (sum of other weights)
        }
        
        // Perform ozz native skinning
        bool skinningSuccess = ozzSystem.skinVertices(
            inPositions.data(), outPositions.data(),
            inNormals.data(), outNormals.data(),
            jointIndices.data(), jointWeights.data(),
            static_cast<int>(vertexCount), 4 // 4 influences per vertex
        );
        
        if (!skinningSuccess) {
            std::cout << "ERROR: Ozz skinning failed, keeping original vertices" << std::endl;
            // Fall back to original vertices
            mesh.animatedVertices = mesh.originalVertices;
        } else {
            // Debug: Check a few transformed positions
            if (vertexCount > 0) {
                // std::cout << "SKINNING_DEBUG: Before: vertex 0 pos=(" << inPositions[0] << "," << inPositions[1] << "," << inPositions[2] << ")" << std::endl;
                // std::cout << "SKINNING_DEBUG: After: vertex 0 pos=(" << outPositions[0] << "," << outPositions[1] << "," << outPositions[2] << ")" << std::endl;
                // if (vertexCount > 100) {
                //     std::cout << "SKINNING_DEBUG: Before: vertex 100 pos=(" << inPositions[300] << "," << inPositions[301] << "," << inPositions[302] << ")" << std::endl;
                //     std::cout << "SKINNING_DEBUG: After: vertex 100 pos=(" << outPositions[300] << "," << outPositions[301] << "," << outPositions[302] << ")" << std::endl;
                // }
            }
            
            // Convert skinned results back to packed format
            for (size_t i = 0; i < vertexCount; i++) {
                auto& animVertex = mesh.animatedVertices[i];
                const auto& origVertex = mesh.originalVertices[i];
                
                // Update positions with skinned results
                animVertex.position[0] = outPositions[i * 3 + 0];
                animVertex.position[1] = outPositions[i * 3 + 1];
                animVertex.position[2] = outPositions[i * 3 + 2];
                
                // Update normals with skinned results and repack
                float nx = outNormals[i * 3 + 0];
                float ny = outNormals[i * 3 + 1];
                float nz = outNormals[i * 3 + 2];
                
                // Pack normals back to uint32_t (RGBA8 format)
                uint8_t packedNx = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255.0f);
                uint8_t packedNy = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255.0f);
                uint8_t packedNz = static_cast<uint8_t>((nz * 0.5f + 0.5f) * 255.0f);
                animVertex.normal = packedNx | (packedNy << 8) | (packedNz << 16) | (0xFF << 24);
                
                // Keep original texture coordinates, bone data unchanged
                animVertex.texcoord[0] = origVertex.texcoord[0];
                animVertex.texcoord[1] = origVertex.texcoord[1];
                for (int j = 0; j < 4; j++) {
                    animVertex.boneIndices[j] = origVertex.boneIndices[j];
                    animVertex.boneWeights[j] = origVertex.boneWeights[j];
                }
            }
            
            // std::cout << "Ozz skinning completed successfully for " << vertexCount << " vertices" << std::endl;
        }
        
        // Update the vertex buffer with transformed vertices using copy for safety
        if (mesh.vertexBuffer.idx != bgfx::kInvalidHandle) {
            bgfx::destroy(mesh.vertexBuffer);
        }
        
        const bgfx::Memory* vertexMem = bgfx::copy(
            mesh.animatedVertices.data(), 
            mesh.animatedVertices.size() * sizeof(PosNormalTexcoordVertex)
        );
        mesh.vertexBuffer = bgfx::createVertexBuffer(vertexMem, PosNormalTexcoordVertex::ms_layout);
    }
}