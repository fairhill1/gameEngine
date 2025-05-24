#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/geometry/runtime/skinning_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/containers/vector.h>
#include <string>
#include <map>
#include <memory>

class OzzAnimationSystem {
public:
    OzzAnimationSystem() = default;
    ~OzzAnimationSystem() = default;
    
    // Load skeleton and animation from ozz files
    bool loadSkeleton(const std::string& skeletonPath);
    bool loadAnimation(const std::string& animationPath);
    bool loadAnimation(const std::string& name, const std::string& animationPath);
    
    // Update animation and get bone matrices
    void updateAnimation(float deltaTime);
    void calculateBoneMatrices(float* outMatrices, size_t maxMatrices);
    
    // Set inverse bind matrices for proper skinning
    void setInverseBindMatrices(const float* inverseBindMatrices, int numJoints);
    void setInverseBindMatricesWithMapping(const float* gltfInverseBindMatrices, int numGltfJoints, 
                                          const std::vector<int>& gltfToOzzMapping);
    
    // Native ozz skinning
    bool skinVertices(const float* inPositions, float* outPositions,
                     const float* inNormals, float* outNormals,
                     const uint16_t* jointIndices, const float* jointWeights,
                     int vertexCount, int influencesCount);
    
    // Get info
    bool isLoaded() const { return skeletonLoaded && animationLoaded; }
    int getNumBones() const;
    float getAnimationDuration() const;
    
    // Get joint names for mapping
    std::vector<std::string> getJointNames() const;
    
    // Animation control
    void setAnimationTime(float time) { animationTime = time; }
    float getAnimationTime() const { return animationTime; }
    void setLoop(bool loop) { looping = loop; }
    void setCurrentAnimation(const std::string& name);
    std::string getCurrentAnimationName() const { return currentAnimationName; }
    
private:
    ozz::animation::Skeleton skeleton;
    const ozz::animation::Animation* currentAnimation = nullptr;  // Pointer to current active animation
    std::map<std::string, std::unique_ptr<ozz::animation::Animation>> animations;  // All loaded animations
    std::string currentAnimationName = "";
    
    // Runtime data
    ozz::vector<ozz::math::SoaTransform> localTransforms;
    ozz::vector<ozz::math::Float4x4> modelMatrices;
    ozz::vector<ozz::math::Float4x4> skinMatrices;  // Final matrices for skinning (model * inverseBind)
    ozz::vector<ozz::math::Float4x4> inverseBindMatrices; // Inverse bind matrices
    ozz::animation::SamplingJob::Context samplingContext;
    
    bool skeletonLoaded = false;
    bool animationLoaded = false;
    float animationTime = 0.0f;
    bool looping = true;
};