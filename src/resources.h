#pragma once

#include <bx/math.h>
#include <iostream>
#include <cstdint>

// Forward declaration for UIRenderer
class UIRenderer;

// Resource types available in the game
enum class ResourceType {
    COPPER,
    IRON,
    STONE
};

// Resource node that can be mined
struct ResourceNode {
    bx::Vec3 position;
    ResourceType type;
    int health;
    int maxHealth;
    float size;
    bool isActive;
    
    ResourceNode(float x, float y, float z, ResourceType resourceType, int hp = 100);
    
    bool canMine() const;
    int mine(int damage = 25);
    const char* getResourceName() const;
    uint32_t getColor() const;
};

// Player inventory system with UI overlay
struct PlayerInventory {
    int copper = 0;
    int iron = 0;
    int stone = 0;
    bool showOverlay = false;
    
    void addResource(ResourceType type, int amount);
    void printInventory() const;
    void toggleOverlay();
    void renderOverlay(UIRenderer& uiRenderer) const;
};