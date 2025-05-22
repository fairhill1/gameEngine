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
            
            // Get accessors and data
            const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionIt->second];
            const tinygltf::BufferView& positionView = gltfModel.bufferViews[positionAccessor.bufferView];
            const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionView.buffer];
            
            // Prepare vertex data
            size_t vertexCount = positionAccessor.count;
            std::vector<PosNormalTexcoordVertex> vertices(vertexCount);
            
            // Extract position data
            for (size_t i = 0; i < vertexCount; i++) {
                size_t offset = positionView.byteOffset + positionAccessor.byteOffset + i * 12; // 3 floats * 4 bytes
                const float* pos = reinterpret_cast<const float*>(&positionBuffer.data[offset]);
                
                // Store position data with scaling applied directly to vertex positions
                // Apply a moderate scale factor to make the model smaller
                const float scaleFactor = 0.01f;
                vertices[i].position[0] = pos[0] * scaleFactor;
                vertices[i].position[1] = pos[1] * scaleFactor;
                vertices[i].position[2] = pos[2] * scaleFactor;
                
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
                    // Don't clamp UVs to allow texture repeating
                    vertices[i].texcoord[0] = int16_t(texcoord[0] * 32767.0f);
                    vertices[i].texcoord[1] = int16_t(texcoord[1] * 32767.0f);
                }
            }
            
            // Create vertex buffer
            modelMesh.vertexBuffer = bgfx::createVertexBuffer(
                bgfx::makeRef(vertices.data(), vertices.size() * sizeof(PosNormalTexcoordVertex)),
                PosNormalTexcoordVertex::ms_layout
            );
            
            // Process indices if present
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = gltfModel.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexView.buffer];
                
                modelMesh.indexCount = indexAccessor.count;
                std::vector<uint16_t> indices(modelMesh.indexCount);
                
                // Handle different index types
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    // Already uint16_t
                    for (size_t i = 0; i < modelMesh.indexCount; i++) {
                        size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i * 2;
                        indices[i] = *reinterpret_cast<const uint16_t*>(&indexBuffer.data[offset]);
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    // Convert uint32_t to uint16_t
                    for (size_t i = 0; i < modelMesh.indexCount; i++) {
                        size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i * 4;
                        uint32_t index = *reinterpret_cast<const uint32_t*>(&indexBuffer.data[offset]);
                        indices[i] = static_cast<uint16_t>(index);
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    // Convert uint8_t to uint16_t
                    for (size_t i = 0; i < modelMesh.indexCount; i++) {
                        size_t offset = indexView.byteOffset + indexAccessor.byteOffset + i;
                        uint8_t index = indexBuffer.data[offset];
                        indices[i] = static_cast<uint16_t>(index);
                    }
                } else {
                    std::cerr << "Unsupported index type: " << indexAccessor.componentType << std::endl;
                    continue;
                }
                
                // Set primitive type
                modelMesh.primitiveType = primitive.mode;
                
                // Create index buffer
                modelMesh.indexBuffer = bgfx::createIndexBuffer(
                    bgfx::makeRef(indices.data(), indices.size() * sizeof(uint16_t))
                );
            } else {
                // No indices, create sequential indices
                std::vector<uint16_t> indices(vertexCount);
                for (uint16_t i = 0; i < vertexCount; i++) {
                    indices[i] = i;
                }
                
                modelMesh.indexCount = indices.size();
                modelMesh.primitiveType = primitive.mode;
                modelMesh.indexBuffer = bgfx::createIndexBuffer(
                    bgfx::makeRef(indices.data(), indices.size() * sizeof(uint16_t))
                );
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
        
        // Set texture with intensive debugging
        if (bgfx::isValid(mesh.texture)) {
            fprintf(stderr, "TEXTURE_DEBUG: Rendering mesh %zu with valid texture\n", i);
            bgfx::setTexture(0, texUniform, mesh.texture);
        } else if (bgfx::isValid(fallbackTexture)) {
            fprintf(stderr, "TEXTURE_DEBUG: Rendering mesh %zu with fallback texture\n", i);
            bgfx::setTexture(0, texUniform, fallbackTexture);
        } else {
            fprintf(stderr, "TEXTURE_DEBUG: Rendering mesh %zu with NO texture\n", i);
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
        vertices[i].normal = encodeNormalRgba8(0.0f, 1.0f, 0.0f); // Up vector
        vertices[i].texcoord[0] = 0;
        vertices[i].texcoord[1] = 0;
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