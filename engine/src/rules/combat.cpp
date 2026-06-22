#include "oce/rules/combat.hpp"

#include <algorithm>

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

AttackResult enemy_attack(Rng& rng, const Enemy& enemy, const Player& player, const Item* armor) {
    const int attack_total = rng.roll(2, 6) + enemy.attack;
    const int damage_die = rng.roll(1, 6);
    const int damage_mod = enemy.attack / 2; // floor; enemy.attack is non-negative
    return resolve_attack(attack_total, player_defense(player, armor), damage_die, damage_mod);
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

} // namespace oce
