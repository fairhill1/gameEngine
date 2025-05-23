#pragma once

#include <bx/math.h>
#include <SDL3/SDL.h>

// Forward declarations
struct Player;

// Camera settings constants
static constexpr float CAMERA_MOVE_SPEED = 0.1f;
static constexpr float CAMERA_SPRINT_MULTIPLIER = 3.0f;
static constexpr float CAMERA_ROTATE_SPEED = 0.005f;

// Camera system with movement and rotation controls
struct Camera {
    bx::Vec3 position;
    float yaw;    // Horizontal rotation
    float pitch;  // Vertical rotation
    
    // Mouse control state
    float prevMouseX;
    float prevMouseY;
    bool mouseDown;
    
    Camera();
    
    void setToPlayerBirdsEye(const Player& player);
    void handleKeyboardInput(const bool* keyboardState, float deltaTime);
    void handleMouseButton(const SDL_Event& event);
    void handleMouseMotion(const SDL_Event& event);
    void getViewMatrix(float* viewMatrix) const;
    void getForwardVector(float* forward) const;
    void getRightVector(float* right) const;
};