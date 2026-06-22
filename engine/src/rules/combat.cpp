#include "oce/rules/combat.hpp"

#include "oce/rules/leveling.hpp"

#include <algorithm>
#include <string>

namespace oce {
namespace {

int floor_div2(int x) {
    return (x >= 0) ? (x / 2) : -(((-x) + 1) / 2);
}

// Flavourful combat-log lines, varied deterministically by the roll value so no
// extra RNG is drawn and resolution stays reproducible.
std::string player_hit_line(const std::string& e, int dmg, int seed) {
    const std::string d = std::to_string(dmg);
    const int v = (seed % 3 + 3) % 3;
    if (dmg >= 6) {
        if (v == 0) return "You crash through " + e + "'s guard for " + d + " damage!";
        if (v == 1) return "A savage blow staggers " + e + " for " + d + " damage!";
        return "You hammer " + e + " for a brutal " + d + " damage!";
    }
    if (dmg >= 3) {
        if (v == 0) return "Your strike bites into " + e + " for " + d + " damage.";
        if (v == 1) return "You land a clean hit on " + e + " for " + d + " damage.";
        return "Steel meets flesh; " + e + " takes " + d + " damage.";
    }
    if (v == 0) return "You graze " + e + " for " + d + " damage.";
    if (v == 1) return "A glancing blow catches " + e + " for " + d + " damage.";
    return "You nick " + e + ", dealing " + d + " damage.";
}

std::string player_miss_line(const std::string& e, int seed) {
    switch ((seed % 4 + 4) % 4) {
        case 0:
            return e + " twists away from your strike.";
        case 1:
            return "Your blow glances off " + e + "'s guard.";
        case 2:
            return e + " parries, and your attack finds only air.";
        default:
            return "You overreach, and " + e + " slips aside.";
    }
}

std::string defeat_line(const std::string& e, int seed) {
    switch ((seed % 3 + 3) % 3) {
        case 0:
            return e + " crumples and does not rise.";
        case 1:
            return "With a final blow, you fell " + e + ".";
        default:
            return e + " collapses, defeated.";
    }
}

std::string enemy_hit_line(const std::string& e, int dmg, int seed) {
    const std::string d = std::to_string(dmg);
    switch ((seed % 4 + 4) % 4) {
        case 0:
            return e + " tears into you for " + d + " damage.";
        case 1:
            return "You reel as " + e + " strikes you for " + d + " damage.";
        case 2:
            return e + " catches you off guard for " + d + " damage.";
        default:
            return e + "'s attack lands hard — " + d + " damage.";
    }
}

std::string enemy_miss_line(const std::string& e, int seed) {
    switch ((seed % 3 + 3) % 3) {
        case 0:
            return e + " lunges, but you sidestep.";
        case 1:
            return "You turn aside " + e + "'s attack.";
        default:
            return e + " swings wide and misses.";
    }
}

} // namespace

EnemyBaseStats enemy_base_stats(int level) {
    if (level < 1) {
        level = 1;
    }
    EnemyBaseStats s;
    s.hp = 6 + level * 4;
    s.attack = 1 + level / 2;        // floor(level/2)
    s.defense = 6 + (level * 3) / 2; // 6 + floor(level * 1.5)
    return s;
}

Enemy make_enemy(Rng& rng, int level, const std::string& id, const std::string& name,
                 const std::string& description) {
    const EnemyBaseStats base = enemy_base_stats(level);
    Enemy e;
    e.id = id;
    e.name = name;
    e.description = description;
    e.hp = base.hp + rng.between(-2, 2);
    e.max_hp = e.hp;
    e.attack = std::max(0, base.attack + rng.between(-1, 1));
    e.defense = base.defense + rng.between(-2, 2);
    return e;
}

int encounter_size(int player_level) {
    if (player_level < 1) {
        player_level = 1;
    }
    return std::min(1 + player_level / 3, 4);
}

int player_attack_bonus(const Player& player, const Item* weapon) {
    const int base = std::max(modifier(player.attributes.strength), modifier(player.attributes.dexterity));
    const int weapon_bonus = (weapon != nullptr) ? weapon->effects.strength : 0;
    return base + weapon_bonus;
}

int player_defense(const Player& player, const Item* armor) {
    const int armor_bonus = (armor != nullptr) ? armor->effects.constitution : 0;
    return 10 + modifier(player.attributes.dexterity) + armor_bonus;
}

int defend_bonus(const Player& player) {
    return 2 + std::max(0, modifier(player.attributes.constitution));
}

AttackResult resolve_attack(int attack_total, int target_defense, int damage_die, int damage_mod) {
    AttackResult r;
    r.total = attack_total;
    r.hit = attack_total >= target_defense;
    r.damage = r.hit ? std::max(1, damage_die + damage_mod) : 0;
    return r;
}

AttackResult player_attack(Rng& rng, const Player& player, Enemy& target, const Item* weapon) {
    const int d1 = rng.between(1, 6);
    const int d2 = rng.between(1, 6);
    const int attack_total = d1 + d2 + player_attack_bonus(player, weapon);
    const int damage_die = rng.roll(1, 6);
    const int damage_mod = modifier(player.attributes.strength);
    AttackResult r = resolve_attack(attack_total, target.defense, damage_die, damage_mod);
    r.dice = {d1, d2};
    if (r.hit) {
        target.hp = std::max(0, target.hp - r.damage);
        r.target_defeated = target.hp <= 0;
    }
    return r;
}

AttackResult enemy_attack(Rng& rng, const Enemy& enemy, const Player& player, const Item* armor,
                          int extra_defense) {
    const int attack_total = rng.roll(2, 6) + enemy.attack;
    const int damage_die = rng.roll(1, 6);
    const int damage_mod = enemy.attack / 2; // floor; enemy.attack is non-negative
    return resolve_attack(attack_total, player_defense(player, armor) + extra_defense, damage_die,
                          damage_mod);
}

bool resolve_flee(int flee_total) {
    return flee_total >= 10;
}

bool flee_check(Rng& rng, const Player& player) {
    const int total = rng.roll(2, 6) + modifier(player.attributes.dexterity) +
                      modifier(player.attributes.luck);
    return resolve_flee(total);
}

long long xp_reward(const Enemy& enemy) {
    const int base = (enemy.max_hp + enemy.attack + enemy.defense) / 3;
    return base > 10 ? (long long) base : 10;
}

int gold_reward(Rng& rng, int player_luck) {
    const int base = rng.between(5, 14);
    const int luck_bonus = floor_div2(modifier(player_luck));
    return std::max(1, base + luck_bonus);
}

bool enemy_action_from_string(const std::string& s, EnemyAction& out) {
    if (s == "attack") {
        out = EnemyAction::Attack;
    } else if (s == "defend") {
        out = EnemyAction::Defend;
    } else {
        return false;
    }
    return true;
}

CombatTurnResult resolve_player_action(GameState& s, Rng& rng, CombatAction action,
                                       int target_index) {
    CombatTurnResult result;
    CombatState& c = s.combat;
    if (!c.active) {
        result.combat_ended = true;
        return result;
    }

    const Item* weapon = s.equipment.hand.has_value() ? &s.equipment.hand.value() : nullptr;
    c.player_guard = 0;

    if (action == CombatAction::Flee) {
        if (flee_check(rng, s.player)) {
            c.log.push_back("You break away and flee the fight.");
            c.active = false;
            c.enemies.clear();
            c.turn = "player";
            result.combat_ended = true;
            result.outcome = CombatOutcomeType::Fled;
            return result;
        }
        c.log.push_back("You try to break away, but cannot escape!");
    } else if (action == CombatAction::Defend) {
        c.player_guard = defend_bonus(s.player);
        c.log.push_back("You raise your guard, bracing for the next blow.");
    } else if (action == CombatAction::Attack && !c.enemies.empty()) {
        if (target_index < 0 || target_index >= (int) c.enemies.size()) {
            target_index = 0;
        }
        const size_t idx = (size_t) target_index;
        const std::string enemy_name = c.enemies[idx].name;
        const int enemy_def = c.enemies[idx].defense;
        const AttackResult r = player_attack(rng, s.player, c.enemies[idx], weapon);
        result.attack_made = true;
        result.attack_dice = r.dice;
        result.attack_total = r.total;
        int dice_sum = 0;
        for (int d : r.dice) {
            dice_sum += d;
        }
        result.attack_modifier = r.total - dice_sum;
        result.attack_target = enemy_def;
        result.attack_label = "Attack: " + enemy_name + " (DEF " + std::to_string(enemy_def) + ")";
        if (!r.hit) {
            c.log.push_back(player_miss_line(enemy_name, r.total));
        } else {
            c.log.push_back(player_hit_line(enemy_name, r.damage, r.total));
            if (r.target_defeated) {
                const long long xp = xp_reward(c.enemies[idx]);
                const int gold = gold_reward(rng, s.player.attributes.luck);
                s.player.xp += xp;
                s.player.gold += gold;
                const int levels = apply_level_up(s.player);
                result.xp_awarded += xp;
                result.gold_awarded += gold;
                result.levels_gained += levels;
                c.log.push_back(defeat_line(enemy_name, r.total) + " (+" + std::to_string(xp) +
                                " xp, +" + std::to_string(gold) + " gold)");
                if (levels > 0) {
                    c.log.push_back("You reach level " + std::to_string(s.player.level) +
                                    "! Your wounds knit and your resolve hardens.");
                }
                c.enemies.erase(c.enemies.begin() + target_index);
            }
        }
    }

    if (c.enemies.empty()) {
        c.log.push_back("The last foe falls. Victory is yours!");
        c.active = false;
        c.turn = "player";
        result.combat_ended = true;
        result.outcome = CombatOutcomeType::Victory;
    }
    return result;
}

CombatTurnResult resolve_enemy_phase(GameState& s, Rng& rng,
                                     const std::vector<EnemyAction>& actions) {
    CombatTurnResult result;
    CombatState& c = s.combat;
    if (!c.active || c.enemies.empty()) {
        return result;
    }

    const Item* armor = s.equipment.body.has_value() ? &s.equipment.body.value() : nullptr;
    const int extra_defense = c.player_guard;
    c.player_guard = 0;

    for (size_t i = 0; i < c.enemies.size(); ++i) {
        Enemy& e = c.enemies[i];
        const EnemyAction act = (i < actions.size()) ? actions[i] : EnemyAction::Attack;
        if (act == EnemyAction::Defend) {
            const int heal = std::max(1, e.max_hp / 10);
            const int before = e.hp;
            e.hp = std::min(e.max_hp, e.hp + heal);
            const int gained = e.hp - before;
            if (gained > 0) {
                c.log.push_back(e.name + " braces and recovers " + std::to_string(gained) +
                                " health.");
            } else {
                c.log.push_back(e.name + " holds back defensively.");
            }
            continue;
        }
        const AttackResult er = enemy_attack(rng, e, s.player, armor, extra_defense);
        if (!er.hit) {
            c.log.push_back(enemy_miss_line(e.name, er.total));
            continue;
        }
        s.player.hp = std::max(0, s.player.hp - er.damage);
        c.log.push_back(enemy_hit_line(e.name, er.damage, er.total));
        if (s.player.hp <= 0) {
            c.log.push_back("You collapse, the world fading to black...");
            c.active = false;
            c.enemies.clear();
            c.turn = "player";
            result.combat_ended = true;
            result.outcome = CombatOutcomeType::Defeat;
            return result;
        }
    }
    return result;
}

CombatTurnResult resolve_combat_turn(GameState& s, Rng& rng, CombatAction action, int target_index) {
    CombatTurnResult result = resolve_player_action(s, rng, action, target_index);
    if (result.combat_ended) {
        return result;
    }
    const CombatTurnResult enemy = resolve_enemy_phase(s, rng, {});
    if (enemy.combat_ended) {
        result.combat_ended = true;
        result.outcome = enemy.outcome;
    }
    return result;
}

} // namespace oce
