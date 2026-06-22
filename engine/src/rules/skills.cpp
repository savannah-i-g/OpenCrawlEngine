#include "oce/rules/skills.hpp"

#include "oce/rules/character.hpp"

namespace oce {

SkillCheckResult perform_skill_check(int roll_total, int modifier_value, int difficulty) {
    SkillCheckResult r;
    r.total = roll_total + modifier_value;
    r.success = r.total >= difficulty;
    r.margin = r.total - difficulty;
    return r;
}

SkillCheckResult roll_skill_check(Rng& rng, int num_dice, int modifier_value, int difficulty) {
    if (num_dice < 1) {
        num_dice = 1;
    }
    return perform_skill_check(rng.roll(num_dice, 6), modifier_value, difficulty);
}

SpellGate can_cast_spell(const Player& player) {
    if (player.cls != CharacterClass::Mage && player.cls != CharacterClass::Cleric) {
        return {false, std::string("As a ") + class_to_string(player.cls) + ", you cannot cast spells."};
    }
    if (player.energy < 15) {
        return {false, "You need 15 energy to cast a spell."};
    }
    return {true, ""};
}

int spell_difficulty(int level) {
    if (level < 1) {
        level = 1;
    }
    return 10 + level / 2;
}

int attribute_value(const Attributes& a, const std::string& name) {
    if (name == "strength") return a.strength;
    if (name == "dexterity") return a.dexterity;
    if (name == "intelligence") return a.intelligence;
    if (name == "constitution") return a.constitution;
    if (name == "wisdom") return a.wisdom;
    if (name == "charisma") return a.charisma;
    if (name == "luck") return a.luck;
    if (name == "perception") return a.perception;
    if (name == "stealth") return a.stealth;
    if (name == "bartering") return a.bartering;
    return 10;
}

int difficulty_dc_offset(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return -2;
        case Difficulty::Normal:
            return 0;
        case Difficulty::Hard:
            return 2;
        case Difficulty::Deadly:
            return 4;
    }
    return 0;
}

int difficulty_level_offset(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return -1;
        case Difficulty::Normal:
            return 0;
        case Difficulty::Hard:
            return 1;
        case Difficulty::Deadly:
            return 2;
    }
    return 0;
}

} // namespace oce
