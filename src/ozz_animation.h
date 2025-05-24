#pragma once

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/geometry/runtime/skinning_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/containers/vector.h>
#include <string>

class OzzAnimationSystem {
public:
    OzzAnimationSystem() = default;
    ~OzzAnimationSystem() = default;
    
    // Load skeleton and animation from ozz files
    bool loadSkeleton(const std::string& skeletonPath);
    bool loadAnimation(const std::string& animationPath);
    
    // Update animation and get bone matrices
    void updateAnimation(float deltaTime);
    void calculateBoneMatrices(float* outMatrices, size_t maxMatrices);
    
    // Native ozz skinning
    bool skinVertices(const float* inPositions, float* outPositions,
                     const float* inNormals, float* outNormals,
                     const uint16_t* jointIndices, const float* jointWeights,
                     int vertexCount, int influencesCount);
    
    // Get info
    bool isLoaded() const { return skeletonLoaded && animationLoaded; }
    int getNumBones() const;
    float getAnimationDuration() const;
    
    // Animation control
    void setAnimationTime(float time) { animationTime = time; }
    float getAnimationTime() const { return animationTime; }
    void setLoop(bool loop) { looping = loop; }
    
private:
    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animation;
    
    // Runtime data
    ozz::vector<ozz::math::SoaTransform> localTransforms;
    ozz::vector<ozz::math::Float4x4> modelMatrices;
    ozz::animation::SamplingJob::Context samplingContext;
    
    bool skeletonLoaded = false;
    bool animationLoaded = false;
    float animationTime = 0.0f;
    bool looping = true;
};