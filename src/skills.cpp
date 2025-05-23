#include "skills.h"
#include "ui.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>

// Skill implementation
Skill::Skill(const std::string& skillName, int startLevel) 
    : name(skillName), level(startLevel), experience(0.0f) {
    calculateExpToNextLevel();
}

void Skill::calculateExpToNextLevel() {
    // Simple exponential curve for leveling
    experienceToNextLevel = 100.0f * bx::pow(1.5f, (float)(level - 1));
}

void Skill::addExperience(float exp) {
    experience += exp;
    while (experience >= experienceToNextLevel) {
        experience -= experienceToNextLevel;
        level++;
        calculateExpToNextLevel();
        std::cout << name << " leveled up to " << level << "!" << std::endl;
    }
}

float Skill::getModifier() const {
    // Each level gives 5% improvement
    return 1.0f + (level - 1) * 0.05f;
}

// PlayerSkills implementation
PlayerSkills::PlayerSkills() {
    // Initialize skills
    skills.emplace(SkillType::ATHLETICS, Skill("Athletics", 1));
    skills.emplace(SkillType::UNARMED, Skill("Unarmed", 1));
    skills.emplace(SkillType::MINING, Skill("Mining", 1));
}

void PlayerSkills::toggleOverlay() {
    showOverlay = !showOverlay;
    std::cout << "Skills overlay " << (showOverlay ? "enabled" : "disabled") << std::endl;
}

void PlayerSkills::renderOverlay(UIRenderer& uiRenderer, float screenHeight) const {
    if (!showOverlay) return;
    
    // Position skills in bottom left corner with custom UI
    float panelX = 10.0f;
    float panelY = screenHeight - 200.0f; // 200px from bottom
    float panelWidth = 200.0f;
    float panelHeight = 180.0f;
    
    // Render skills panel background (black with transparency)
    uiRenderer.panel(panelX, panelY, panelWidth, panelHeight, 0xAA000000);
    
    // Render skills header
    uiRenderer.text(panelX + 10, panelY + 20, "=== SKILLS ===", UIColors::TEXT_HIGHLIGHT);
    
    // Render each skill with proper spacing
    float lineY = panelY + 50;
    for (const auto& [type, skill] : skills) {
        // Skill name and level
        char skillText[64];
        snprintf(skillText, sizeof(skillText), "%s Lv.%d", skill.name.c_str(), skill.level);
        uiRenderer.text(panelX + 10, lineY, skillText, UIColors::TEXT_NORMAL);
        
        // XP progress
        int xpPercent = (int)((skill.experience / skill.experienceToNextLevel) * 100.0f);
        snprintf(skillText, sizeof(skillText), "  XP: %d/%d (%d%%)", 
                (int)skill.experience, (int)skill.experienceToNextLevel, xpPercent);
        uiRenderer.text(panelX + 10, lineY + 25, skillText, UIColors::GRAY);
        
        lineY += 45; // Space between skills
    }
    
    // Render footer instruction
    uiRenderer.text(panelX + 10, panelY + panelHeight - 25, "Press C to close", UIColors::GRAY);
}

Skill& PlayerSkills::getSkill(SkillType type) {
    return skills.at(type);
}