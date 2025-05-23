#include "player.h"
#include "npcs.h"
#include "ui.h"
#include <bgfx/bgfx.h>
#include <cstdlib>

// Player implementation
Player::Player() : position({0.0f, 0.0f, 0.0f}), targetPosition({0.0f, 0.0f, 0.0f}), 
           hasTarget(false), isSprinting(false), moveSpeed(0.05f), sprintSpeed(0.15f), size(0.3f),
           health(100), maxHealth(100), lastDamageTime(0.0f),
           combatTarget(nullptr), lastAttackTime(0.0f), attackCooldown(1.2f),
           attackDamage(15), hitChance(0.8f), dodgeChance(0.3f), inCombat(false), hitFlashTimer(0.0f),
           lastMovementTime(0.0f), distanceTraveled(0.0f) {}

// setTarget and update methods implemented in main.cpp due to ChunkManager dependency

void Player::takeDamage(int damage, float currentTime) {
    if (canTakeDamage(currentTime)) {
        health = bx::max(0, health - damage);
        lastDamageTime = currentTime;
        hitFlashTimer = 0.2f; // Flash red for 0.2 seconds
        std::cout << "Player took " << damage << " damage! Health: " << health << "/" << maxHealth << std::endl;
        
        if (health <= 0) {
            std::cout << "Player defeated! Respawning..." << std::endl;
            respawn();
        }
    }
}

bool Player::canTakeDamage(float currentTime) const {
    return (currentTime - lastDamageTime) > 1.0f; // 1 second damage immunity
}

void Player::heal(int amount) {
    health = bx::min(maxHealth, health + amount);
    std::cout << "Player healed " << amount << " HP! Health: " << health << "/" << maxHealth << std::endl;
}

void Player::respawn() {
    health = maxHealth;
    position = {0.0f, 0.0f, 0.0f}; // Reset to spawn point
    hasTarget = false;
    lastDamageTime = 0.0f;
}

void Player::renderHealthBar(UIRenderer& uiRenderer, float screenWidth) const {
    // Calculate health percentage for color coding
    float healthPercent = (float)health / (float)maxHealth;
    
    // Choose color based on health percentage
    uint32_t healthColor;
    if (healthPercent > 0.7f) healthColor = UIColors::WHITE; // White for healthy
    else if (healthPercent > 0.3f) healthColor = UIColors::TEXT_WARNING; // Yellow for wounded
    else healthColor = UIColors::TEXT_ERROR; // Red for critical
    
    // Position in top-center of screen
    char healthText[32];
    snprintf(healthText, sizeof(healthText), "Health: %d/%d", health, maxHealth);
    
    // Center the text based on screen width
    float textX = (screenWidth - 120.0f) / 2.0f; // Approximate centering for health text
    float textY = 15.0f; // Top area
    
    uiRenderer.text(textX, textY, healthText, healthColor);
}