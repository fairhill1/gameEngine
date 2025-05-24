#include "ozz_animation.h"
#include <ozz/base/io/stream.h>
#include <ozz/base/io/archive.h>
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
}

void OzzAnimationSystem::calculateBoneMatrices(float* outMatrices, size_t maxMatrices) {
    if (!isLoaded()) return;
    
    const size_t numMatrices = std::min(maxMatrices, static_cast<size_t>(modelMatrices.size()));
    
    for (size_t i = 0; i < numMatrices; i++) {
        const ozz::math::Float4x4& matrix = modelMatrices[i];
        
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