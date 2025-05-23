#include "resources.h"
#include <bgfx/bgfx.h>

// ResourceNode implementation
ResourceNode::ResourceNode(float x, float y, float z, ResourceType resourceType, int hp) 
    : position({x, y, z}), type(resourceType), health(hp), maxHealth(hp), size(0.5f), isActive(true) {}

bool ResourceNode::canMine() const {
    return isActive && health > 0;
}

int ResourceNode::mine(int damage) {
    if (!canMine()) return 0;
    
    health -= damage;
    std::cout << "Mining " << getResourceName() << " - Health: " << health << "/" << maxHealth << std::endl;
    
    if (health <= 0) {
        health = 0;
        isActive = false;
        std::cout << getResourceName() << " node depleted! Gained 1 " << getResourceName() << std::endl;
        return 1; // Return resource gained
    }
    return 0;
}

const char* ResourceNode::getResourceName() const {
    switch (type) {
        case ResourceType::COPPER: return "Copper";
        case ResourceType::IRON: return "Iron";
        case ResourceType::STONE: return "Stone";
        default: return "Unknown";
    }
}

uint32_t ResourceNode::getColor() const {
    switch (type) {
        case ResourceType::COPPER: return 0xff4A90E2; // Orange-brown for copper
        case ResourceType::IRON: return 0xff808080;   // Gray for iron
        case ResourceType::STONE: return 0xff606060;  // Dark gray for stone
        default: return 0xffffffff;
    }
}

// PlayerInventory implementation
void PlayerInventory::addResource(ResourceType type, int amount) {
    switch (type) {
        case ResourceType::COPPER: copper += amount; break;
        case ResourceType::IRON: iron += amount; break;
        case ResourceType::STONE: stone += amount; break;
    }
    printInventory();
}

void PlayerInventory::printInventory() const {
    std::cout << "=== INVENTORY ===" << std::endl;
    std::cout << "Copper: " << copper << std::endl;
    std::cout << "Iron: " << iron << std::endl;
    std::cout << "Stone: " << stone << std::endl;
    std::cout << "=================" << std::endl;
}

void PlayerInventory::toggleOverlay() {
    showOverlay = !showOverlay;
    std::cout << "Inventory overlay " << (showOverlay ? "enabled" : "disabled") << std::endl;
}

void PlayerInventory::renderOverlay() const {
    if (!showOverlay) return;
    
    // Position inventory in top left corner
    int x = 1; // 1 character from left edge
    int y = 1; // 1 character from top edge
    
    // Render inventory header
    bgfx::dbgTextPrintf(x, y, 0x0f, "=== INVENTORY ===");
    
    // Render resource counts with color coding
    bgfx::dbgTextPrintf(x, y + 1, 0x06, "Copper: %d", copper);  // Orange/brown color
    bgfx::dbgTextPrintf(x, y + 2, 0x08, "Iron:   %d", iron);    // Gray color
    bgfx::dbgTextPrintf(x, y + 3, 0x07, "Stone:  %d", stone);   // Light gray color
    
    // Render footer
    bgfx::dbgTextPrintf(x, y + 4, 0x0f, "=================");
    bgfx::dbgTextPrintf(x, y + 5, 0x0a, "Press I to close");    // Green color for instruction
}