#include "model.h"
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
                        // Flip V coordinate since glTF and BGFX may use different conventions
                        vertices[i].texcoord[0] = int16_t(texcoord[0] * 32767.0f);
                        vertices[i].texcoord[1] = int16_t((1.0f - texcoord[1]) * 32767.0f); // Flip V
                        
                        // Debug first few texture coordinates
                        if (i < 3) {
                            fprintf(stderr, "TEXCOORD_DEBUG: Vertex %zu: UV=(%.6f, %.6f) -> (%d, %d)\n", 
                                   i, texcoord[0], texcoord[1], vertices[i].texcoord[0], vertices[i].texcoord[1]);
                        }
                    } else {
                        // Safe fallback for out-of-bounds access
                        vertices[i].texcoord[0] = 0;
                        vertices[i].texcoord[1] = 0;
                        fprintf(stderr, "WARNING: Texture coordinate buffer bounds exceeded at vertex %zu\n", i);
                    }
                }
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
}