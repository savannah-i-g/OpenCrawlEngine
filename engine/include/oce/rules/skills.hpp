#pragma once
// Skill checks and the spellcasting gate.

#include "oce/model.hpp"
#include "oce/rules/dice.hpp"

#include <string>
#include <vector>

namespace oce {

struct SkillCheckResult {
    bool success = false;
    int total = 0;
    int margin = 0;        // total - difficulty
    std::vector<int> dice; // individual d6 results
};

// Pure: total = roll_total + modifier; success when total >= difficulty.
SkillCheckResult perform_skill_check(int roll_total, int modifier_value, int difficulty);
// Rolls num_dice d6, then resolves.
SkillCheckResult roll_skill_check(Rng& rng, int num_dice, int modifier_value, int difficulty);

// Outcome tier from the margin: a margin of ±5 marks a critical result.
enum class CheckTier { CriticalFailure, Failure, Success, CriticalSuccess };
CheckTier check_tier(const SkillCheckResult& result);

struct SpellGate {
    bool allowed = false;
    std::string reason; // empty when allowed
};

// Only mages and clerics may cast, and only with at least 15 energy.
SpellGate can_cast_spell(const Player& player);
// Spell skill-check difficulty: 10 + floor(level / 2).
int spell_difficulty(int level);

// Looks up a named attribute's value (10 if the name is unknown).
int attribute_value(const Attributes& attributes, const std::string& name);

// Difficulty offsets: applied to skill-check DCs and to spawned enemy levels.
int difficulty_dc_offset(Difficulty difficulty);    // easy -2, normal 0, hard +2, deadly +4
int difficulty_level_offset(Difficulty difficulty); // easy -1, normal 0, hard +1, deadly +2

} // namespace oce
