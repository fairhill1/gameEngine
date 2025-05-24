#include "ozz_animation.h"
#include <ozz/base/io/stream.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/span.h>
#include <iostream>

bool OzzAnimationSystem::loadSkeleton(const std::string& skeletonPath) {
    ozz::io::File file(skeletonPath.c_str(), "rb");
    if (!file.opened()) {
        std::cerr << "Failed to open skeleton file: " << skeletonPath << std::endl;
        return false;
    }
    
    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Skeleton>()) {
        std::cerr << "Invalid skeleton file format" << std::endl;
        return false;
    }
    
    archive >> skeleton;
    
    // Allocate runtime buffers
    const int numJoints = skeleton.num_joints();
    localTransforms.resize(numJoints);
    modelMatrices.resize(numJoints);
    
    // Allocate sampling context
    samplingContext.Resize(numJoints);
    
    skeletonLoaded = true;
    std::cout << "Loaded skeleton with " << numJoints << " joints" << std::endl;
    return true;
}

bool OzzAnimationSystem::loadAnimation(const std::string& animationPath) {
    ozz::io::File file(animationPath.c_str(), "rb");
    if (!file.opened()) {
        std::cerr << "Failed to open animation file: " << animationPath << std::endl;
        return false;
    }
    
    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Animation>()) {
        std::cerr << "Invalid animation file format" << std::endl;
        return false;
    }
    
    archive >> animation;
    
    animationLoaded = true;
    std::cout << "Loaded animation with duration: " << animation.duration() << "s" << std::endl;
    return true;
}

void OzzAnimationSystem::updateAnimation(float deltaTime) {
    if (!isLoaded()) return;
    
    // Update animation time
    animationTime += deltaTime;
    
    // Handle looping
    if (looping && animationTime > animation.duration()) {
        animationTime = fmod(animationTime, animation.duration());
    }
    
    // Sample animation
    ozz::animation::SamplingJob samplingJob;
    samplingJob.animation = &animation;
    samplingJob.context = &samplingContext;
    samplingJob.ratio = animationTime / animation.duration();
    samplingJob.output = make_span(localTransforms);
    
    if (!samplingJob.Run()) {
        std::cerr << "Animation sampling failed" << std::endl;
        return;
    }
    
    // Convert to model space matrices
    ozz::animation::LocalToModelJob localToModelJob;
    localToModelJob.skeleton = &skeleton;
    localToModelJob.input = make_span(localTransforms);
    localToModelJob.output = make_span(modelMatrices);
    
    if (!localToModelJob.Run()) {
        std::cerr << "Local to model conversion failed" << std::endl;
        return;
    }
    
    // DEBUG: Test with identity transforms to isolate joint ordering issue
    static bool useIdentityTransforms = false; // Disabled - now implement joint mapping
    
    if (useIdentityTransforms) {
        // Use identity transforms - this should show bind pose even with animation enabled
        skinMatrices.resize(modelMatrices.size());
        for (size_t i = 0; i < modelMatrices.size(); i++) {
            // Create identity matrix
            skinMatrices[i].cols[0] = ozz::math::simd_float4::Load(1.0f, 0.0f, 0.0f, 0.0f);
            skinMatrices[i].cols[1] = ozz::math::simd_float4::Load(0.0f, 1.0f, 0.0f, 0.0f);
            skinMatrices[i].cols[2] = ozz::math::simd_float4::Load(0.0f, 0.0f, 1.0f, 0.0f);
            skinMatrices[i].cols[3] = ozz::math::simd_float4::Load(0.0f, 0.0f, 0.0f, 1.0f);
        }
        std::cout << "DEBUG: Using identity transforms to test joint mapping" << std::endl;
    } else {
        // Compute skin matrices by multiplying model matrices with inverse bind matrices
        if (inverseBindMatrices.size() == modelMatrices.size()) {
            skinMatrices.resize(modelMatrices.size());
            for (size_t i = 0; i < modelMatrices.size(); i++) {
                skinMatrices[i] = modelMatrices[i] * inverseBindMatrices[i];
            }
            std::cout << "Using proper skin matrices (model * inverse bind)" << std::endl;
        } else {
            // If no inverse bind matrices, use model matrices directly
            skinMatrices = modelMatrices;
            std::cout << "Size mismatch - using model matrices directly (inv:" << inverseBindMatrices.size() << " vs model:" << modelMatrices.size() << ")" << std::endl;
        }
    }
}

void OzzAnimationSystem::calculateBoneMatrices(float* outMatrices, size_t maxMatrices) {
    if (!isLoaded()) return;
    
    const size_t numMatrices = std::min(maxMatrices, static_cast<size_t>(skinMatrices.size()));
    
    for (size_t i = 0; i < numMatrices; i++) {
        const ozz::math::Float4x4& matrix = skinMatrices[i];
        
        // Copy matrix data (ozz uses column-major, BGFX expects column-major too)
        float* dest = outMatrices + (i * 16);
        
        // Access SIMD components properly
        dest[0] = ozz::math::GetX(matrix.cols[0]);   dest[4] = ozz::math::GetX(matrix.cols[1]);   dest[8] = ozz::math::GetX(matrix.cols[2]);   dest[12] = ozz::math::GetX(matrix.cols[3]);
        dest[1] = ozz::math::GetY(matrix.cols[0]);   dest[5] = ozz::math::GetY(matrix.cols[1]);   dest[9] = ozz::math::GetY(matrix.cols[2]);   dest[13] = ozz::math::GetY(matrix.cols[3]);
        dest[2] = ozz::math::GetZ(matrix.cols[0]);   dest[6] = ozz::math::GetZ(matrix.cols[1]);   dest[10] = ozz::math::GetZ(matrix.cols[2]);  dest[14] = ozz::math::GetZ(matrix.cols[3]);
        dest[3] = ozz::math::GetW(matrix.cols[0]);   dest[7] = ozz::math::GetW(matrix.cols[1]);   dest[11] = ozz::math::GetW(matrix.cols[2]);  dest[15] = ozz::math::GetW(matrix.cols[3]);
    }
}

int OzzAnimationSystem::getNumBones() const {
    return skeletonLoaded ? skeleton.num_joints() : 0;
}

float OzzAnimationSystem::getAnimationDuration() const {
    return animationLoaded ? animation.duration() : 0.0f;
}

std::vector<std::string> OzzAnimationSystem::getJointNames() const {
    std::vector<std::string> names;
    if (!skeletonLoaded) return names;
    
    const auto& jointNames = skeleton.joint_names();
    names.reserve(jointNames.size());
    
    for (const auto& name : jointNames) {
        names.push_back(std::string(name));
    }
    
    return names;
}

void OzzAnimationSystem::setInverseBindMatrices(const float* inverseBindMatrices, int numJoints) {
    // Resize to match the skeleton size, padding with identity matrices if needed
    int skeletonJoints = skeleton.num_joints();
    this->inverseBindMatrices.resize(skeletonJoints);
    
    for (int i = 0; i < skeletonJoints; i++) {
        ozz::math::Float4x4& ozzMatrix = this->inverseBindMatrices[i];
        
        if (i < numJoints) {
            // Use provided inverse bind matrix
            const float* matrix = inverseBindMatrices + (i * 16);
            
            // Convert from column-major float array to ozz::math::Float4x4
            ozzMatrix.cols[0] = ozz::math::simd_float4::Load(matrix[0], matrix[1], matrix[2], matrix[3]);      // Column 0
            ozzMatrix.cols[1] = ozz::math::simd_float4::Load(matrix[4], matrix[5], matrix[6], matrix[7]);      // Column 1
            ozzMatrix.cols[2] = ozz::math::simd_float4::Load(matrix[8], matrix[9], matrix[10], matrix[11]);    // Column 2
            ozzMatrix.cols[3] = ozz::math::simd_float4::Load(matrix[12], matrix[13], matrix[14], matrix[15]);  // Column 3
        } else {
            // Pad with identity matrix for missing joints
            ozzMatrix.cols[0] = ozz::math::simd_float4::Load(1.0f, 0.0f, 0.0f, 0.0f);  // Column 0
            ozzMatrix.cols[1] = ozz::math::simd_float4::Load(0.0f, 1.0f, 0.0f, 0.0f);  // Column 1
            ozzMatrix.cols[2] = ozz::math::simd_float4::Load(0.0f, 0.0f, 1.0f, 0.0f);  // Column 2
            ozzMatrix.cols[3] = ozz::math::simd_float4::Load(0.0f, 0.0f, 0.0f, 1.0f);  // Column 3
        }
    }
    
    std::cout << "Set " << numJoints << " inverse bind matrices, padded to " << skeletonJoints << " for ozz skeleton" << std::endl;
}

void OzzAnimationSystem::setInverseBindMatricesWithMapping(const float* gltfInverseBindMatrices, int numGltfJoints, 
                                                          const std::vector<int>& gltfToOzzMapping) {
    // Resize to match the skeleton size, initialize with identity matrices
    int skeletonJoints = skeleton.num_joints();
    this->inverseBindMatrices.resize(skeletonJoints);
    
    // Initialize all matrices to identity
    for (int i = 0; i < skeletonJoints; i++) {
        ozz::math::Float4x4& ozzMatrix = this->inverseBindMatrices[i];
        ozzMatrix.cols[0] = ozz::math::simd_float4::Load(1.0f, 0.0f, 0.0f, 0.0f);  // Column 0
        ozzMatrix.cols[1] = ozz::math::simd_float4::Load(0.0f, 1.0f, 0.0f, 0.0f);  // Column 1
        ozzMatrix.cols[2] = ozz::math::simd_float4::Load(0.0f, 0.0f, 1.0f, 0.0f);  // Column 2
        ozzMatrix.cols[3] = ozz::math::simd_float4::Load(0.0f, 0.0f, 0.0f, 1.0f);  // Column 3
    }
    
    // Map glTF inverse bind matrices to their corresponding ozz joint positions
    int mappedCount = 0;
    for (int gltfIdx = 0; gltfIdx < numGltfJoints && gltfIdx < (int)gltfToOzzMapping.size(); gltfIdx++) {
        int ozzIdx = gltfToOzzMapping[gltfIdx];
        if (ozzIdx >= 0 && ozzIdx < skeletonJoints) {
            // Copy the glTF inverse bind matrix to the correct ozz joint position
            const float* matrix = gltfInverseBindMatrices + (gltfIdx * 16);
            ozz::math::Float4x4& ozzMatrix = this->inverseBindMatrices[ozzIdx];
            
            // Convert from column-major float array to ozz::math::Float4x4
            ozzMatrix.cols[0] = ozz::math::simd_float4::Load(matrix[0], matrix[1], matrix[2], matrix[3]);      // Column 0
            ozzMatrix.cols[1] = ozz::math::simd_float4::Load(matrix[4], matrix[5], matrix[6], matrix[7]);      // Column 1
            ozzMatrix.cols[2] = ozz::math::simd_float4::Load(matrix[8], matrix[9], matrix[10], matrix[11]);    // Column 2
            ozzMatrix.cols[3] = ozz::math::simd_float4::Load(matrix[12], matrix[13], matrix[14], matrix[15]);  // Column 3
            
            mappedCount++;
            std::cout << "INVERSE_BIND_DEBUG: Mapped glTF[" << gltfIdx << "] inverse bind -> ozz[" << ozzIdx << "]" << std::endl;
        }
    }
    
    std::cout << "Set " << mappedCount << "/" << numGltfJoints << " inverse bind matrices with proper joint mapping" << std::endl;
}

bool OzzAnimationSystem::skinVertices(const float* inPositions, float* outPositions,
                                     const float* inNormals, float* outNormals,
                                     const uint16_t* jointIndices, const float* jointWeights,
                                     int vertexCount, int influencesCount) {
    if (!isLoaded()) {
        return false;
    }
    
    // Create ozz skinning job
    ozz::geometry::SkinningJob skinningJob;
    
    // Set vertex count and influences
    skinningJob.vertex_count = vertexCount;
    skinningJob.influences_count = influencesCount;
    
    // Set joint matrices (these are the final skin matrices: model * inverseBind)
    skinningJob.joint_matrices = ozz::make_span(skinMatrices);
    
    // Set input positions and normals using pointer constructor
    skinningJob.in_positions = ozz::span<const float>(inPositions, vertexCount * 3);
    skinningJob.in_positions_stride = sizeof(float) * 3;
    
    if (inNormals && outNormals) {
        skinningJob.in_normals = ozz::span<const float>(inNormals, vertexCount * 3);
        skinningJob.in_normals_stride = sizeof(float) * 3;
    }
    
    // Set joint indices and weights
    skinningJob.joint_indices = ozz::span<const uint16_t>(jointIndices, vertexCount * influencesCount);
    skinningJob.joint_indices_stride = sizeof(uint16_t) * influencesCount;
    
    skinningJob.joint_weights = ozz::span<const float>(jointWeights, vertexCount * (influencesCount - 1));
    skinningJob.joint_weights_stride = sizeof(float) * (influencesCount - 1);
    
    // Set output positions and normals
    skinningJob.out_positions = ozz::span<float>(outPositions, vertexCount * 3);
    skinningJob.out_positions_stride = sizeof(float) * 3;
    
    if (inNormals && outNormals) {
        skinningJob.out_normals = ozz::span<float>(outNormals, vertexCount * 3);
        skinningJob.out_normals_stride = sizeof(float) * 3;
    }
    
    // Validate and run the skinning job
    if (!skinningJob.Validate()) {
        std::cerr << "Ozz skinning job validation failed" << std::endl;
        return false;
    }
    
    return skinningJob.Run();
}