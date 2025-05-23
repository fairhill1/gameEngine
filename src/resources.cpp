#include "resources.h"
#include "ui.h"
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

void PlayerInventory::renderOverlay(UIRenderer& uiRenderer) const {
    if (!showOverlay) return;
    
    // Position inventory in top left corner with custom UI
    float panelX = 10.0f;
    float panelY = 10.0f;
    float panelWidth = 180.0f;
    float panelHeight = 140.0f;
    
    // Render inventory panel background (black with transparency)
    uiRenderer.panel(panelX, panelY, panelWidth, panelHeight, 0xAA000000);
    
    // Render inventory header
    uiRenderer.text(panelX + 10, panelY + 20, "=== INVENTORY ===", UIColors::TEXT_HIGHLIGHT);
    
    // Render resource counts with proper spacing
    char inventoryText[64];
    snprintf(inventoryText, sizeof(inventoryText), "Copper: %d", copper);
    uiRenderer.text(panelX + 10, panelY + 50, inventoryText, UIColors::TEXT_NORMAL);
    
    snprintf(inventoryText, sizeof(inventoryText), "Iron:   %d", iron);
    uiRenderer.text(panelX + 10, panelY + 75, inventoryText, UIColors::TEXT_NORMAL);
    
    snprintf(inventoryText, sizeof(inventoryText), "Stone:  %d", stone);
    uiRenderer.text(panelX + 10, panelY + 100, inventoryText, UIColors::TEXT_NORMAL);
    
    // Render footer instruction
    uiRenderer.text(panelX + 10, panelY + 125, "Press I to close", UIColors::GRAY);
}