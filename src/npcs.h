#pragma once

#include <bx/math.h>
#include <iostream>
#include <cstdint>

// Forward declarations
struct Player;

// NPC types available in the game
enum class NPCType {
    WANDERER,
    VILLAGER,
    MERCHANT
};

// NPC AI states
enum class NPCState {
    WANDERING,
    IDLE,
    MOVING_TO_TARGET,
    APPROACHING_ENEMY,  // Moving toward combat target
    IN_COMBAT,          // Actively fighting
    FLEEING             // Running away (for villagers)
};

// Non-player character with AI behavior
struct NPC {
    bx::Vec3 position;
    bx::Vec3 velocity;
    bx::Vec3 targetPosition;
    NPCType type;
    NPCState state;
    float speed;
    float size;
    float stateTimer;
    float maxStateTime;
    bool isActive;
    uint32_t color;
    uint32_t baseColor;      // Original color for health calculation
    int health;
    int maxHealth;
    bool isHostile;
    float lastDamageTime;
    
    // Combat properties
    Player* combatTarget;    // Who we're fighting
    float lastAttackTime;
    float attackCooldown;    // Time between attacks
    float attackRange;       // How close to attack
    float aggroRange;        // Detection range
    float combatRange;       // Preferred fighting distance
    int attackDamage;
    float hitChance;         // 0.0 to 1.0
    float dodgeChance;       // 0.0 to 1.0
    float hitFlashTimer;     // Red flash when hit
    
    NPC(float x, float y, float z, NPCType npcType);
    
    // Update method implemented in main.cpp due to circular dependency with Player
    void update(float deltaTime, float terrainHeight, Player* player, float currentTime);
    const char* getTypeName() const;
    void takeDamage(int damage, float currentTime);
    void updateHealthColor();
    bool canTakeDamage(float currentTime) const;
    void heal(int amount);
};