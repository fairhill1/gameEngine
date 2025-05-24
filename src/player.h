#pragma once

#include <bx/math.h>
#include <iostream>
#include "skills.h"

// Forward declarations
struct NPC;
class ChunkManager;
class UIRenderer;

// Player character with movement, combat, and skills
struct Player {
    bx::Vec3 position;
    bx::Vec3 targetPosition;
    bool hasTarget;
    bool isSprinting;
    float moveSpeed;
    float sprintSpeed;
    float size;
    int health;
    int maxHealth;
    float lastDamageTime;
    
    // Orientation and rotation
    float rotation;          // Current rotation in radians (Y-axis)
    float targetRotation;    // Target rotation for smooth transitions
    float rotationSpeed;     // How fast to rotate (radians per second)
    
    // Combat properties
    NPC* combatTarget;       // Current NPC we're fighting
    float lastAttackTime;
    float attackCooldown;    // Time between attacks
    int attackDamage;
    float hitChance;         // 0.0 to 1.0
    float dodgeChance;       // 0.0 to 1.0
    bool inCombat;
    float hitFlashTimer;     // Red flash when hit
    
    // Skills
    PlayerSkills skills;
    float lastMovementTime;
    float distanceTraveled;
    
    Player();
    
    void setTarget(float x, float z, const ChunkManager& chunkManager, bool sprint = false);
    void update(const ChunkManager& chunkManager, float currentTime, float deltaTime);
    void takeDamage(int damage, float currentTime);
    bool canTakeDamage(float currentTime) const;
    void heal(int amount);
    void respawn();
    void renderHealthBar(UIRenderer& uiRenderer, float screenWidth) const;
};