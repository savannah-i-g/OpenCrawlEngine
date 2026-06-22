#pragma once
// Turn-based combat rules. Formula functions are pure; the Rng-driven helpers
// roll the dice and apply results.

#include "oce/model.hpp"
#include "oce/rules/dice.hpp"

#include <string>

namespace oce {

struct EnemyBaseStats {
    int hp;
    int attack;
    int defense;
};

// Deterministic base stats for an enemy of the given level.
EnemyBaseStats enemy_base_stats(int level);

// A fully-statted enemy of the given level, with small random variation. The
// enemy starts at full health (hp == max_hp).
Enemy make_enemy(Rng& rng, int level, const std::string& id, const std::string& name,
                 const std::string& description);

// How many enemies a level-appropriate encounter spawns: min(1 + level/3, 4).
int encounter_size(int player_level);

// Player attack bonus = max(str mod, dex mod) + weapon strength bonus.
int player_attack_bonus(const Player& player, const Item* weapon);
// Player defense = 10 + dex mod + armor constitution bonus.
int player_defense(const Player& player, const Item* armor);
// Temporary defense from the defend action: 2 + max(0, con mod).
int defend_bonus(const Player& player);

struct AttackResult {
    bool hit = false;
    int total = 0;          // attack roll + bonus
    int damage = 0;         // damage dealt on a hit
    bool target_defeated = false;
};

// Pure resolution: a hit lands when attack_total >= target_defense; damage is
// then max(1, damage_die + damage_mod).
AttackResult resolve_attack(int attack_total, int target_defense, int damage_die, int damage_mod);

// Rolls 2d6 + bonus to hit and 1d6 + str mod for damage, applying it to target.
AttackResult player_attack(Rng& rng, const Player& player, Enemy& target, const Item* weapon);
// Rolls the enemy's attack; the caller applies result.damage to the player.
// extra_defense adds to the player's defense for this attack (e.g. while defending).
AttackResult enemy_attack(Rng& rng, const Enemy& enemy, const Player& player, const Item* armor,
                          int extra_defense = 0);

// Flee succeeds when 2d6 + dex mod + luck mod >= 10.
bool resolve_flee(int flee_total); // flee_total already includes the modifiers
bool flee_check(Rng& rng, const Player& player);

// Rewards for defeating an enemy.
long long xp_reward(const Enemy& enemy);    // max(10, floor((maxHp + atk + def)/3))
int       gold_reward(Rng& rng, int player_luck); // (5..14) + floor(luck mod / 2), min 1

// One player combat action, resolved against the live combat state.
enum class CombatAction { Attack, Defend, Flee };

struct CombatTurnResult {
    bool combat_ended = false;
    CombatOutcomeType outcome = CombatOutcomeType::Victory; // valid when combat_ended
    long long xp_awarded = 0;
    int gold_awarded = 0;
    int levels_gained = 0;
};

// Resolves the player's action and the ensuing enemy turns: mutates s.player and
// s.combat, awards xp/gold (with leveling) as enemies fall, ends combat on
// victory/defeat/successful flee, and appends readable lines to s.combat.log.
CombatTurnResult resolve_combat_turn(GameState& s, Rng& rng, CombatAction action, int target_index);

} // namespace oce
