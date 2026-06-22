#include "oce/rules/combat.hpp"

#include "oce/rules/leveling.hpp"

#include <algorithm>
#include <string>

namespace oce {
namespace {

int floor_div2(int x) {
    return (x >= 0) ? (x / 2) : -(((-x) + 1) / 2);
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
    const int attack_total = rng.roll(2, 6) + player_attack_bonus(player, weapon);
    const int damage_die = rng.roll(1, 6);
    const int damage_mod = modifier(player.attributes.strength);
    AttackResult r = resolve_attack(attack_total, target.defense, damage_die, damage_mod);
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

CombatTurnResult resolve_combat_turn(GameState& s, Rng& rng, CombatAction action, int target_index) {
    CombatTurnResult result;
    CombatState& c = s.combat;
    if (!c.active) {
        result.combat_ended = true;
        return result;
    }

    const Item* weapon = s.equipment.hand.has_value() ? &s.equipment.hand.value() : nullptr;
    const Item* armor = s.equipment.body.has_value() ? &s.equipment.body.value() : nullptr;
    int extra_defense = 0;

    if (action == CombatAction::Flee) {
        if (flee_check(rng, s.player)) {
            c.log.push_back("You flee from the fight.");
            c.active = false;
            c.enemies.clear();
            c.turn = "player";
            result.combat_ended = true;
            result.outcome = CombatOutcomeType::Fled;
            return result;
        }
        c.log.push_back("You fail to escape.");
    } else if (action == CombatAction::Defend) {
        extra_defense = defend_bonus(s.player);
        c.log.push_back("You take a defensive stance.");
    } else if (action == CombatAction::Attack && !c.enemies.empty()) {
        if (target_index < 0 || target_index >= (int) c.enemies.size()) {
            target_index = 0;
        }
        const size_t idx = (size_t) target_index;
        const std::string enemy_name = c.enemies[idx].name;
        const AttackResult r = player_attack(rng, s.player, c.enemies[idx], weapon);
        if (!r.hit) {
            c.log.push_back("You miss " + enemy_name + ".");
        } else {
            c.log.push_back("You hit " + enemy_name + " for " + std::to_string(r.damage) +
                            " damage.");
            if (r.target_defeated) {
                const long long xp = xp_reward(c.enemies[idx]);
                const int gold = gold_reward(rng, s.player.attributes.luck);
                s.player.xp += xp;
                s.player.gold += gold;
                const int levels = apply_level_up(s.player);
                result.xp_awarded += xp;
                result.gold_awarded += gold;
                result.levels_gained += levels;
                c.log.push_back("You defeat " + enemy_name + ". (+" + std::to_string(xp) + " xp, +" +
                                std::to_string(gold) + " gold)");
                if (levels > 0) {
                    c.log.push_back("You reach level " + std::to_string(s.player.level) + "!");
                }
                c.enemies.erase(c.enemies.begin() + target_index);
            }
        }
    }

    if (c.enemies.empty()) {
        c.log.push_back("Victory!");
        c.active = false;
        c.turn = "player";
        result.combat_ended = true;
        result.outcome = CombatOutcomeType::Victory;
        return result;
    }

    for (Enemy& e : c.enemies) {
        const AttackResult er = enemy_attack(rng, e, s.player, armor, extra_defense);
        if (!er.hit) {
            c.log.push_back(e.name + " misses you.");
            continue;
        }
        s.player.hp = std::max(0, s.player.hp - er.damage);
        c.log.push_back(e.name + " hits you for " + std::to_string(er.damage) + " damage.");
        if (s.player.hp <= 0) {
            c.log.push_back("You have fallen.");
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

} // namespace oce
