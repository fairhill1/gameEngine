#include "skills.h"
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

void PlayerSkills::renderOverlay() const {
    if (!showOverlay) return;
    
    // Position skills in bottom left corner
    int x = 1;
    int y = 25; // Bottom left, safe for 600px window (about 37 rows total)
    
    // Render skills header with black background
    bgfx::dbgTextPrintf(x, y, 0x0f, "=== SKILLS ===");  // white text on black background
    
    // Render each skill
    int lineOffset = 1;
    for (const auto& [type, skill] : skills) {
        int xpPercent = (int)((skill.experience / skill.experienceToNextLevel) * 100.0f);
        bgfx::dbgTextPrintf(x, y + lineOffset, 0x0b, "%s Lv.%d", skill.name.c_str(), skill.level);  // cyan on black
        bgfx::dbgTextPrintf(x, y + lineOffset + 1, 0x03, "  XP: %d/%d (%d%%)", 
                           (int)skill.experience, (int)skill.experienceToNextLevel, xpPercent);  // cyan on black
        lineOffset += 2;
    }
    
    // Render footer
    bgfx::dbgTextPrintf(x, y + lineOffset, 0x0f, "===============");  // white on black
    bgfx::dbgTextPrintf(x, y + lineOffset + 1, 0x0a, "Press C to close");  // green on black
}

Skill& PlayerSkills::getSkill(SkillType type) {
    return skills.at(type);
}