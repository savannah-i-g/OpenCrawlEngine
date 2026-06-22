// Deterministic tests for the combat rules, against the reference formulas.
#include "oce/rules/combat.hpp"

#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

int main(void) {
    using namespace oce;

    // Enemy base stats: hp = 6 + 4*level, attack = 1 + level/2, defense = 6 + floor(1.5*level).
    EnemyBaseStats l1 = enemy_base_stats(1);
    CHECK(l1.hp == 10 && l1.attack == 1 && l1.defense == 7);
    EnemyBaseStats l2 = enemy_base_stats(2);
    CHECK(l2.hp == 14 && l2.attack == 2 && l2.defense == 9);
    EnemyBaseStats l3 = enemy_base_stats(3);
    CHECK(l3.hp == 18 && l3.attack == 2 && l3.defense == 10);
    EnemyBaseStats l5 = enemy_base_stats(5);
    CHECK(l5.hp == 26 && l5.attack == 3 && l5.defense == 13);

    CHECK(encounter_size(1) == 1);
    CHECK(encounter_size(3) == 2);
    CHECK(encounter_size(6) == 3);
    CHECK(encounter_size(9) == 4);
    CHECK(encounter_size(12) == 4); // capped

    // Attack bonus and defense.
    Player p;
    p.attributes.strength = 14;  // mod +2
    p.attributes.dexterity = 10; // mod 0
    p.attributes.constitution = 14; // mod +2
    p.attributes.luck = 10;
    CHECK(player_attack_bonus(p, nullptr) == 2);
    Item sword;
    sword.kind = ItemKind::Weapon;
    sword.effects.strength = 3;
    CHECK(player_attack_bonus(p, &sword) == 5);
    CHECK(player_defense(p, nullptr) == 10); // 10 + dex mod(0)
    Item armor;
    armor.kind = ItemKind::Armor;
    armor.effects.constitution = 2;
    CHECK(player_defense(p, &armor) == 12); // 10 + 0 + armor(2)
    Player nimble;
    nimble.attributes.dexterity = 16; // mod +3
    CHECK(player_defense(nimble, nullptr) == 13);
    CHECK(defend_bonus(p) == 4);
    Player weak;
    weak.attributes.constitution = 8; // mod -1
    CHECK(defend_bonus(weak) == 2);

    // Pure attack resolution.
    AttackResult hit = resolve_attack(10, 7, 4, 2);
    CHECK(hit.hit && hit.damage == 6);
    AttackResult miss = resolve_attack(5, 7, 4, 2);
    CHECK(!miss.hit && miss.damage == 0);
    AttackResult floored = resolve_attack(10, 7, 1, -5);
    CHECK(floored.hit && floored.damage == 1); // damage is never below 1 on a hit

    // XP reward: max(10, floor((maxHp + atk + def)/3)).
    Enemy e;
    e.max_hp = 18;
    e.attack = 2;
    e.defense = 10;
    CHECK(xp_reward(e) == 10); // 30/3 = 10
    e.max_hp = 30;
    e.attack = 5;
    e.defense = 12;
    CHECK(xp_reward(e) == 15); // 47/3 = 15
    e.max_hp = 6;
    e.attack = 1;
    e.defense = 6;
    CHECK(xp_reward(e) == 10); // 13/3 = 4 -> floored up to the minimum 10

    // Rng-driven attack: against defense 0 every swing hits; damage stays in
    // [1, 6 + str mod] and reduces the target until defeated.
    Rng rng(123u);
    Enemy dummy;
    dummy.defense = 0;
    dummy.hp = 60;
    dummy.max_hp = 60;
    int guard = 0;
    while (dummy.hp > 0 && guard < 1000) {
        int before = dummy.hp;
        AttackResult r = player_attack(rng, p, dummy, nullptr);
        CHECK(r.hit);
        CHECK(r.damage >= 1 && r.damage <= 8); // 1d6 + str mod(+2)
        CHECK(dummy.hp == before - r.damage || dummy.hp == 0);
        ++guard;
    }
    CHECK(dummy.hp == 0);

    // make_enemy varies around the base and starts at full health.
    Rng rng2(999u);
    for (int i = 0; i < 50; ++i) {
        Enemy m = make_enemy(rng2, 3, "e", "Goblin", "a goblin");
        CHECK(m.hp == m.max_hp);
        CHECK(m.hp >= 16 && m.hp <= 20);     // 18 +/- 2
        CHECK(m.attack >= 1 && m.attack <= 3); // max(0, 2 +/- 1)
        CHECK(m.defense >= 8 && m.defense <= 12); // 10 +/- 2
    }

    // Flee threshold.
    CHECK(resolve_flee(10));
    CHECK(!resolve_flee(9));

    // Gold reward stays positive and in range for neutral luck.
    Rng rng3(5u);
    for (int i = 0; i < 200; ++i) {
        int g = gold_reward(rng3, 10); // luck mod 0 -> base 5..14
        CHECK(g >= 5 && g <= 14);
    }
    Rng rng4(6u);
    for (int i = 0; i < 200; ++i) {
        int g = gold_reward(rng4, 1); // luck mod -5 -> bonus -3, but never below 1
        CHECK(g >= 1);
    }

    if (failures == 0) {
        printf("combat: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "combat: %d checks failed\n", failures);
    return 1;
}
