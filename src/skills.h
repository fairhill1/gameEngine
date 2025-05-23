#pragma once

#include <unordered_map>
#include <string>
#include <iostream>

// Forward declaration for UIRenderer
class UIRenderer;

// Skill types available in the game
enum class SkillType {
    ATHLETICS,
    UNARMED,
    MINING
};

// Individual skill with leveling system
struct Skill {
    std::string name;
    int level;
    float experience;
    float experienceToNextLevel;
    
    Skill(const std::string& skillName, int startLevel = 1);
    
    void calculateExpToNextLevel();
    void addExperience(float exp);
    float getModifier() const;
};

// Player skills manager with UI overlay
struct PlayerSkills {
    std::unordered_map<SkillType, Skill> skills;
    bool showOverlay = false;
    
    PlayerSkills();
    
    void toggleOverlay();
    void renderOverlay(UIRenderer& uiRenderer, float screenHeight) const;
    Skill& getSkill(SkillType type);
};