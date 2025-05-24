#include "npcs.h"
#include "player.h"
#include <cstdlib>

// NPC implementation
NPC::NPC(float x, float y, float z, NPCType npcType) 
    : position({x, y, z}), velocity({0.0f, 0.0f, 0.0f}), targetPosition({x, y, z}),
      type(npcType), state(NPCState::IDLE), speed(1.5f), size(0.8f), 
      stateTimer(0.0f), maxStateTime(3.0f), isActive(true), isHostile(false), lastDamageTime(0.0f),
      combatTarget(nullptr), lastAttackTime(0.0f), attackCooldown(1.5f), attackRange(1.5f), 
      aggroRange(8.0f), combatRange(2.5f), attackDamage(10), hitChance(0.7f), dodgeChance(0.2f), hitFlashTimer(0.0f) {
    
    // Set NPC-specific properties
    switch (type) {
        case NPCType::WANDERER:
            speed = 2.0f;
            baseColor = 0xff00AA00; // Green
            maxHealth = 60;         // Medium health
            maxStateTime = 2.0f + (bx::sin(x * 0.1f) * 0.5f + 0.5f) * 3.0f; // 2-5 seconds
            // Combat stats
            attackDamage = 8;
            hitChance = 0.65f;
            dodgeChance = 0.25f;
            attackCooldown = 1.8f;
            break;
        case NPCType::VILLAGER:
            speed = 1.0f;
            baseColor = 0xff0066FF; // Blue  
            maxHealth = 40;         // Low health (peaceful)
            maxStateTime = 4.0f + (bx::cos(z * 0.1f) * 0.5f + 0.5f) * 4.0f; // 4-8 seconds
            // Combat stats (villagers flee instead of fight)
            attackDamage = 0;
            hitChance = 0.0f;
            dodgeChance = 0.4f;  // Good at dodging while fleeing
            aggroRange = 12.0f;  // Detect threats from farther away
            break;
        case NPCType::MERCHANT:
            speed = 1.5f;
            baseColor = 0xffFFAA00; // Orange
            maxHealth = 80;         // High health (well-armed traders)
            maxStateTime = 3.0f + (bx::sin((x + z) * 0.1f) * 0.5f + 0.5f) * 2.0f; // 3-5 seconds
            // Combat stats (better equipped)
            attackDamage = 12;
            hitChance = 0.75f;
            dodgeChance = 0.15f;
            attackCooldown = 1.5f;
            break;
    }
    
    health = maxHealth;  // Start at full health
    color = baseColor;   // Start with base color
    updateHealthColor(); // Apply initial color
    
    // Initialize animation system (model is now shared globally)
    if (!ozzAnimSystem.loadSkeleton("build/assets/skeleton.ozz")) {
        std::cerr << "Failed to load skeleton for NPC!" << std::endl;
    }
    if (!ozzAnimSystem.loadAnimation("idle", "build/assets/Armature_mixamo.com_Layer0.002.ozz")) {
        std::cerr << "Failed to load idle animation for NPC!" << std::endl;
    }
    if (!ozzAnimSystem.loadAnimation("walking", "build/assets/walking_inplace.ozz")) {
        std::cerr << "Failed to load walking animation for NPC!" << std::endl;
    }
    
    // Set animation to idle by default
    ozzAnimSystem.setCurrentAnimation("idle");
    
    // Note: Joint mapping will be set up globally for the shared model
}

const char* NPC::getTypeName() const {
    switch (type) {
        case NPCType::WANDERER: return "Wanderer";
        case NPCType::VILLAGER: return "Villager";
        case NPCType::MERCHANT: return "Merchant";
        default: return "Unknown";
    }
}

void NPC::takeDamage(int damage, float currentTime) {
    health = bx::max(0, health - damage);
    lastDamageTime = currentTime;
    hitFlashTimer = 0.2f; // Flash red for 0.2 seconds
    updateHealthColor();
    
    // Make NPC hostile when attacked (except villagers who flee)
    if (type != NPCType::VILLAGER) {
        isHostile = true;
    }
    
    std::cout << getTypeName() << " took " << damage << " damage! Health: " << health << "/" << maxHealth;
    if (health <= 0) {
        std::cout << " - " << getTypeName() << " defeated!";
        isActive = false;
    }
    std::cout << std::endl;
}

void NPC::updateHealthColor() {
    if (health <= 0) {
        color = 0xff404040; // Dark gray when dead
        return;
    }
    
    // Flash red when hit
    if (hitFlashTimer > 0) {
        // Interpolate between red and base color based on timer
        float flashIntensity = hitFlashTimer / 0.2f; // 0.2 is max flash duration
        uint32_t r = 255;
        uint32_t g = ((baseColor >> 8) & 0xFF) * (1.0f - flashIntensity);
        uint32_t b = (baseColor & 0xFF) * (1.0f - flashIntensity);
        color = 0xff000000 | (r << 16) | (g << 8) | b;
        return;
    }
    
    float healthPercent = (float)health / (float)maxHealth;
    
    if (isHostile) {
        // Hostile NPCs get red tint
        if (healthPercent > 0.7f) color = 0xffAA0000; // Dark red
        else if (healthPercent > 0.3f) color = 0xff880000; // Darker red
        else color = 0xff660000; // Very dark red
    } else {
        // Damaged but peaceful NPCs get darker versions of base color
        if (healthPercent > 0.7f) {
            color = baseColor; // Full color
        } else if (healthPercent > 0.3f) {
            // 50% darker
            uint32_t r = ((baseColor >> 16) & 0xFF) / 2;
            uint32_t g = ((baseColor >> 8) & 0xFF) / 2;
            uint32_t b = (baseColor & 0xFF) / 2;
            color = 0xff000000 | (r << 16) | (g << 8) | b;
        } else {
            // 75% darker (very dark)
            uint32_t r = ((baseColor >> 16) & 0xFF) / 4;
            uint32_t g = ((baseColor >> 8) & 0xFF) / 4;
            uint32_t b = (baseColor & 0xFF) / 4;
            color = 0xff000000 | (r << 16) | (g << 8) | b;
        }
    }
}

bool NPC::canTakeDamage(float currentTime) const {
    return (currentTime - lastDamageTime) > 0.5f; // 0.5 second damage immunity
}

void NPC::heal(int amount) {
    health = bx::min(maxHealth, health + amount);
    updateHealthColor();
    std::cout << getTypeName() << " healed " << amount << " HP! Health: " << health << "/" << maxHealth << std::endl;
}

void NPC::update(float deltaTime, float terrainHeight, Player* player, float currentTime) {
    if (!isActive) return;
    
    stateTimer += deltaTime;
    
    // Update animation based on movement state
    bool isMoving = (state == NPCState::MOVING_TO_TARGET || 
                     state == NPCState::APPROACHING_ENEMY || 
                     state == NPCState::FLEEING ||
                     state == NPCState::WANDERING);
    
    if (isMoving) {
        ozzAnimSystem.setCurrentAnimation("walking");
    } else {
        ozzAnimSystem.setCurrentAnimation("idle");
    }
    
    // Update animation system
    ozzAnimSystem.updateAnimation(deltaTime);
    
    // Update hit flash timer
    if (hitFlashTimer > 0) {
        hitFlashTimer -= deltaTime;
        if (hitFlashTimer < 0) hitFlashTimer = 0;
    }
    
    // Check for combat initiation if hostile or if player is our combat target
    if (player && (isHostile || combatTarget == player)) {
        float dx = player->position.x - position.x;
        float dz = player->position.z - position.z;
        float distToPlayer = bx::sqrt(dx * dx + dz * dz);
        
        // Check if we should enter combat
        if (distToPlayer <= aggroRange && state != NPCState::IN_COMBAT && state != NPCState::FLEEING) {
            if (type == NPCType::VILLAGER) {
                // Villagers flee instead of fight
                state = NPCState::FLEEING;
                // Set flee target opposite of player
                targetPosition.x = position.x - (dx / distToPlayer) * 15.0f;
                targetPosition.z = position.z - (dz / distToPlayer) * 15.0f;
            } else if (isHostile || combatTarget == player) {
                // Enter combat mode
                state = NPCState::APPROACHING_ENEMY;
                combatTarget = player;
            }
            stateTimer = 0.0f;
        }
    }
    
    switch (state) {
        case NPCState::IDLE:
            if (stateTimer >= maxStateTime) {
                // Pick a new random target within reasonable distance
                float angle = (bx::sin(position.x * 0.7f + stateTimer) * 0.5f + 0.5f) * 2.0f * bx::kPi;
                float distance = 5.0f + (bx::cos(position.z * 0.8f + stateTimer) * 0.5f + 0.5f) * 10.0f; // 5-15 units
                
                targetPosition.x = position.x + bx::cos(angle) * distance;
                targetPosition.z = position.z + bx::sin(angle) * distance;
                
                state = NPCState::MOVING_TO_TARGET;
                stateTimer = 0.0f;
            }
            break;
            
        case NPCState::MOVING_TO_TARGET: {
            // Move towards target
            float dx = targetPosition.x - position.x;
            float dz = targetPosition.z - position.z;
            float distance = bx::sqrt(dx * dx + dz * dz);
            
            if (distance < 1.0f) {
                // Reached target, go idle
                state = NPCState::IDLE;
                stateTimer = 0.0f;
                velocity = {0.0f, 0.0f, 0.0f};
            } else {
                // Move towards target
                velocity.x = (dx / distance) * speed;
                velocity.z = (dz / distance) * speed;
                
                position.x += velocity.x * deltaTime;
                position.z += velocity.z * deltaTime;
                
                // Update Y position to follow terrain
                position.y = terrainHeight - 5.0f + 0.1f; // Mannequin feet should touch ground
            }
            
            // Timeout check - if moving too long, go idle
            if (stateTimer > 15.0f) {
                state = NPCState::IDLE;
                stateTimer = 0.0f;
                velocity = {0.0f, 0.0f, 0.0f};
            }
            break;
        }
        
        case NPCState::APPROACHING_ENEMY: {
            if (combatTarget) {
                float dx = combatTarget->position.x - position.x;
                float dz = combatTarget->position.z - position.z;
                float distance = bx::sqrt(dx * dx + dz * dz);
                
                if (distance <= combatRange) {
                    // Close enough, switch to combat
                    state = NPCState::IN_COMBAT;
                    velocity = {0.0f, 0.0f, 0.0f};
                } else {
                    // Move toward target
                    velocity.x = (dx / distance) * speed * 1.5f; // Move faster when approaching combat
                    velocity.z = (dz / distance) * speed * 1.5f;
                    
                    position.x += velocity.x * deltaTime;
                    position.z += velocity.z * deltaTime;
                    position.y = terrainHeight - 5.0f + 0.1f; // Mannequin feet should touch ground
                }
                
                // Give up if too far or taking too long
                if (distance > aggroRange * 1.5f || stateTimer > 10.0f) {
                    state = NPCState::IDLE;
                    combatTarget = nullptr;
                    isHostile = false;
                    stateTimer = 0.0f;
                }
            }
            break;
        }
        
        case NPCState::IN_COMBAT: {
            if (combatTarget) {
                float dx = combatTarget->position.x - position.x;
                float dz = combatTarget->position.z - position.z;
                float distance = bx::sqrt(dx * dx + dz * dz);
                
                // Maintain combat distance
                if (distance > combatRange + 0.5f) {
                    // Too far, move closer
                    velocity.x = (dx / distance) * speed;
                    velocity.z = (dz / distance) * speed;
                } else if (distance < combatRange - 0.5f) {
                    // Too close, back up
                    velocity.x = -(dx / distance) * speed * 0.5f;
                    velocity.z = -(dz / distance) * speed * 0.5f;
                } else {
                    // Good distance, circle strafe
                    float strafeAngle = currentTime * 0.5f;
                    velocity.x = -dz / distance * speed * 0.3f * bx::sin(strafeAngle);
                    velocity.z = dx / distance * speed * 0.3f * bx::sin(strafeAngle);
                }
                
                position.x += velocity.x * deltaTime;
                position.z += velocity.z * deltaTime;
                position.y = terrainHeight - 5.0f + 0.1f; // Mannequin feet should touch ground
                
                // Attack if cooldown is ready
                if (currentTime - lastAttackTime >= attackCooldown) {
                    // RNG for hit/miss
                    float hitRoll = (rand() % 100) / 100.0f;
                    float dodgeRoll = (rand() % 100) / 100.0f;
                    
                    if (hitRoll < hitChance && dodgeRoll > combatTarget->dodgeChance) {
                        // Hit!
                        combatTarget->takeDamage(attackDamage, currentTime);
                        std::cout << getTypeName() << " hits player for " << attackDamage << " damage!" << std::endl;
                    } else {
                        std::cout << getTypeName() << " misses!" << std::endl;
                    }
                    
                    lastAttackTime = currentTime;
                }
                
                // Exit combat if target is too far or dead
                if (distance > aggroRange * 1.5f || combatTarget->health <= 0) {
                    state = NPCState::IDLE;
                    combatTarget = nullptr;
                    isHostile = false;
                    stateTimer = 0.0f;
                }
            }
            break;
        }
        
        case NPCState::FLEEING: {
            // Run away!
            float dx = targetPosition.x - position.x;
            float dz = targetPosition.z - position.z;
            float distance = bx::sqrt(dx * dx + dz * dz);
            
            if (distance < 2.0f || stateTimer > 8.0f) {
                // Reached safe distance or timeout
                state = NPCState::IDLE;
                stateTimer = 0.0f;
                velocity = {0.0f, 0.0f, 0.0f};
            } else {
                // Sprint away
                velocity.x = (dx / distance) * speed * 2.0f;
                velocity.z = (dz / distance) * speed * 2.0f;
                
                position.x += velocity.x * deltaTime;
                position.z += velocity.z * deltaTime;
                position.y = terrainHeight - 5.0f + 0.1f; // Mannequin feet should touch ground
            }
            break;
        }
            
        case NPCState::WANDERING:
            // Continuous random walk
            if (stateTimer >= 1.0f) {
                float angle = (bx::sin(position.x * 1.1f + stateTimer) + bx::cos(position.z * 0.9f + stateTimer)) * 0.5f;
                velocity.x = bx::cos(angle) * speed * 0.5f;
                velocity.z = bx::sin(angle) * speed * 0.5f;
                stateTimer = 0.0f;
            }
            
            position.x += velocity.x * deltaTime;
            position.z += velocity.z * deltaTime;
            
            // Update Y position to follow terrain  
            position.y = terrainHeight - 5.0f + 0.1f; // Mannequin feet should touch ground
            break;
    }
}

void NPC::setupInverseBindMatrices(const Model& sharedModel) {
    // Extract inverse bind matrices from the shared model
    std::vector<float> inverseBindMatrices;
    if (sharedModel.getInverseBindMatrices(inverseBindMatrices)) {
        int numGltfJoints = inverseBindMatrices.size() / 16;
        auto ozzJointNames = ozzAnimSystem.getJointNames();
        
        // Create joint mapping (same logic as in main.cpp)
        std::vector<int> gltfToOzzMapping(numGltfJoints, -1);
        int mappedCount = 0;
        
        if (!sharedModel.skins.empty()) {
            const auto& skin = sharedModel.skins[0];
            for (int skinIdx = 0; skinIdx < (int)skin.jointIndices.size(); skinIdx++) {
                int nodeIndex = skin.jointIndices[skinIdx];
                if (nodeIndex < (int)sharedModel.nodes.size()) {
                    const std::string& gltfName = sharedModel.nodes[nodeIndex].name;
                    // Skip first 2 ozz joints (Armature, Ch36) - start from Hips
                    for (int ozzIdx = 2; ozzIdx < (int)ozzJointNames.size(); ozzIdx++) {
                        if (gltfName == ozzJointNames[ozzIdx]) {
                            gltfToOzzMapping[skinIdx] = ozzIdx;
                            mappedCount++;
                            break;
                        }
                    }
                }
            }
        }
        
        // Set inverse bind matrices with mapping for this NPC's animation system
        ozzAnimSystem.setInverseBindMatricesWithMapping(inverseBindMatrices.data(), numGltfJoints, gltfToOzzMapping);
        // std::cout << "NPC inverse bind matrices set: " << mappedCount << "/" << numGltfJoints << " joints mapped" << std::endl;
    } else {
        std::cerr << "Failed to extract inverse bind matrices from shared model for NPC!" << std::endl;
    }
}

