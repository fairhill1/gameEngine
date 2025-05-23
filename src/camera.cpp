#include "camera.h"
#include "player.h"
#include <iostream>

// Camera implementation
Camera::Camera() : position({0.0f, 15.0f, 8.0f}), yaw(0.0f), pitch(-0.5f), 
                   prevMouseX(0.0f), prevMouseY(0.0f), mouseDown(false) {}

void Camera::setToPlayerBirdsEye(const Player& player) {
    position.x = player.position.x;
    position.y = player.position.y + 15.0f; // Bird's eye height
    position.z = player.position.z + 8.0f;  // Slightly back from player
    
    // Calculate direction vector from camera to player
    float dirX = player.position.x - position.x;
    float dirY = player.position.y - position.y;
    float dirZ = player.position.z - position.z;
    
    // Calculate horizontal distance for pitch calculation
    float horizontalDist = bx::sqrt(dirX * dirX + dirZ * dirZ);
    
    // Calculate pitch (looking down at player)
    pitch = bx::atan2(-dirY, horizontalDist);
    
    // Calculate yaw (horizontal rotation toward player)
    yaw = bx::atan2(dirX, dirZ);
    
    std::cout << "Camera jumped to bird's eye view of player (pitch: " << pitch << ", yaw: " << yaw << ")" << std::endl;
}

void Camera::handleKeyboardInput(const bool* keyboardState, float deltaTime) {
    // Calculate forward and right vectors
    float forward[3];
    float right[3];
    getForwardVector(forward);
    getRightVector(right);
    
    bool sprinting = keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT];
    float currentSpeed = CAMERA_MOVE_SPEED * (sprinting ? CAMERA_SPRINT_MULTIPLIER : 1.0f);
    
    if (keyboardState[SDL_SCANCODE_W]) {
        position.x += forward[0] * currentSpeed;
        position.y += forward[1] * currentSpeed;
        position.z += forward[2] * currentSpeed;
    }
    if (keyboardState[SDL_SCANCODE_S]) {
        position.x -= forward[0] * currentSpeed;
        position.y -= forward[1] * currentSpeed;
        position.z -= forward[2] * currentSpeed;
    }
    if (keyboardState[SDL_SCANCODE_A]) {
        position.x -= right[0] * currentSpeed;
        position.y -= right[1] * currentSpeed;
        position.z -= right[2] * currentSpeed;
    }
    if (keyboardState[SDL_SCANCODE_D]) {
        position.x += right[0] * currentSpeed;
        position.y += right[1] * currentSpeed;
        position.z += right[2] * currentSpeed;
    }
    if (keyboardState[SDL_SCANCODE_Q]) {
        position.y += currentSpeed;
    }
    if (keyboardState[SDL_SCANCODE_E]) {
        position.y -= currentSpeed;
    }
}

void Camera::handleMouseButton(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouseDown = true;
            SDL_GetMouseState(&prevMouseX, &prevMouseY);
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouseDown = false;
        }
    }
}

void Camera::handleMouseMotion(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_MOTION && mouseDown) {
        float mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        
        float deltaX = mouseX - prevMouseX;
        float deltaY = mouseY - prevMouseY;
        
        yaw += deltaX * CAMERA_ROTATE_SPEED;
        pitch += deltaY * CAMERA_ROTATE_SPEED;
        
        // Clamp pitch to prevent camera flipping
        if (pitch > bx::kPi * 0.49f) pitch = bx::kPi * 0.49f;
        if (pitch < -bx::kPi * 0.49f) pitch = -bx::kPi * 0.49f;
        
        prevMouseX = mouseX;
        prevMouseY = mouseY;
    }
}

void Camera::getViewMatrix(float* viewMatrix) const {
    float forward[3];
    getForwardVector(forward);
    
    bx::Vec3 target = {
        position.x + forward[0],
        position.y + forward[1],
        position.z + forward[2]
    };
    
    bx::mtxLookAt(viewMatrix, position, target);
}

void Camera::getForwardVector(float* forward) const {
    forward[0] = bx::sin(yaw) * bx::cos(pitch);
    forward[1] = -bx::sin(pitch);
    forward[2] = bx::cos(yaw) * bx::cos(pitch);
}

void Camera::getRightVector(float* right) const {
    right[0] = bx::cos(yaw);
    right[1] = 0.0f;
    right[2] = -bx::sin(yaw);
}