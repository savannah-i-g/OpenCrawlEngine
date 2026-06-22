#pragma once
// Skill checks and the spellcasting gate.

#include "oce/model.hpp"
#include "oce/rules/dice.hpp"

#include <string>

namespace oce {

struct SkillCheckResult {
    bool success = false;
    int total = 0;
    int margin = 0; // total - difficulty
};

// Pure: total = roll_total + modifier; success when total >= difficulty.
SkillCheckResult perform_skill_check(int roll_total, int modifier_value, int difficulty);
// Rolls num_dice d6, then resolves.
SkillCheckResult roll_skill_check(Rng& rng, int num_dice, int modifier_value, int difficulty);

struct SpellGate {
    bool allowed = false;
    std::string reason; // empty when allowed
};

// Only mages and clerics may cast, and only with at least 15 energy.
SpellGate can_cast_spell(const Player& player);
// Spell skill-check difficulty: 10 + floor(level / 2).
int spell_difficulty(int level);

} // namespace oce
