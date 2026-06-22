// Deterministic tests for skill checks, item generation/inventory, and the
// character factory, against the reference constants.
#include "oce/rules/character.hpp"
#include "oce/rules/items.hpp"
#include "oce/rules/skills.hpp"

#include <cstdio>
#include <string>
#include <vector>

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

    // Skill checks.
    SkillCheckResult ok = perform_skill_check(8, 2, 10);
    CHECK(ok.total == 10 && ok.success && ok.margin == 0);
    SkillCheckResult bad = perform_skill_check(5, 2, 10);
    CHECK(bad.total == 7 && !bad.success && bad.margin == -3);
    CHECK(spell_difficulty(1) == 10);
    CHECK(spell_difficulty(4) == 12);
    CHECK(spell_difficulty(10) == 15);

    Player warrior;
    warrior.cls = CharacterClass::Warrior;
    CHECK(!can_cast_spell(warrior).allowed);
    Player mage;
    mage.cls = CharacterClass::Mage;
    mage.energy = 20;
    CHECK(can_cast_spell(mage).allowed);
    mage.energy = 10;
    CHECK(!can_cast_spell(mage).allowed);
    Player cleric;
    cleric.cls = CharacterClass::Cleric;
    cleric.energy = 15;
    CHECK(can_cast_spell(cleric).allowed);

    // Rarity bonuses and item generation.
    CHECK(weapon_armor_rarity_bonus(ItemRarity::Common) == 1);
    CHECK(weapon_armor_rarity_bonus(ItemRarity::Legendary) == 5);
    CHECK(potion_rarity_bonus(ItemRarity::Common) == 15);
    CHECK(potion_rarity_bonus(ItemRarity::Legendary) == 100);

    Item w = generate_item("w", "Sword", "", ItemKind::Weapon, ItemSlot::Hand, ItemRarity::Common, 2,
                           false);
    CHECK(w.effects.strength == 3);
    Item wl = generate_item("w2", "Sword", "", ItemKind::Weapon, ItemSlot::Hand,
                            ItemRarity::Legendary, 3, false);
    CHECK(wl.effects.strength == 8);
    Item ar = generate_item("a", "Armor", "", ItemKind::Armor, ItemSlot::Body, ItemRarity::Common, 1,
                            false);
    CHECK(ar.effects.constitution == 2);
    Item hp_pot = generate_item("p", "Potion", "", ItemKind::Potion, ItemSlot::Consumable,
                                ItemRarity::Common, 20, false);
    CHECK(hp_pot.effects.hp == 35 && hp_pot.effects.energy == 0);
    Item en_pot = generate_item("p2", "Potion", "", ItemKind::Potion, ItemSlot::Consumable,
                                ItemRarity::Common, 20, true);
    CHECK(en_pot.effects.energy == 35 && en_pot.effects.hp == 0);

    // Equip / unequip.
    std::vector<Item> inv;
    inv.push_back(generate_item("sw", "Sword", "", ItemKind::Weapon, ItemSlot::Hand,
                                ItemRarity::Common, 2, false));
    inv.push_back(generate_item("ar", "Armor", "", ItemKind::Armor, ItemSlot::Body,
                                ItemRarity::Common, 1, false));
    inv.push_back(generate_item("po", "Potion", "", ItemKind::Potion, ItemSlot::Consumable,
                                ItemRarity::Common, 20, false));
    Equipment eq;
    CHECK(equip_item(inv, eq, "sw"));
    CHECK(eq.hand.has_value() && eq.hand->id == "sw");
    CHECK(inv.size() == 2u);
    CHECK(equip_item(inv, eq, "ar"));
    CHECK(eq.body.has_value());
    CHECK(!equip_item(inv, eq, "po"));   // a potion is not equippable
    CHECK(!equip_item(inv, eq, "none")); // missing id

    inv.push_back(generate_item("sw2", "Sword", "", ItemKind::Weapon, ItemSlot::Hand,
                                ItemRarity::Rare, 2, false));
    CHECK(equip_item(inv, eq, "sw2"));
    CHECK(eq.hand->id == "sw2");
    bool first_sword_returned = false;
    for (const Item& i : inv) {
        if (i.id == "sw") {
            first_sword_returned = true;
        }
    }
    CHECK(first_sword_returned); // the displaced weapon went back to the inventory

    CHECK(unequip_slot(inv, eq, ItemSlot::Body));
    CHECK(!eq.body.has_value());
    CHECK(!unequip_slot(inv, eq, ItemSlot::Body)); // already empty

    // Consume.
    Player p;
    p.hp = 10;
    p.max_hp = 50;
    std::vector<Item> inv2;
    inv2.push_back(generate_item("hpot", "Health Potion", "", ItemKind::Potion,
                                 ItemSlot::Consumable, ItemRarity::Common, 20, false)); // hp 35
    CHECK(consume_item(p, inv2, "hpot"));
    CHECK(p.hp == 45); // 10 + 35
    CHECK(inv2.empty());
    CHECK(!consume_item(p, inv2, "hpot")); // already consumed

    Player p2;
    p2.hp = 40;
    p2.max_hp = 50;
    std::vector<Item> inv3;
    inv3.push_back(generate_item("h", "Potion", "", ItemKind::Potion, ItemSlot::Consumable,
                                 ItemRarity::Common, 20, false));
    CHECK(consume_item(p2, inv3, "h"));
    CHECK(p2.hp == 50); // clamped to max

    Player p3;
    std::vector<Item> inv4;
    inv4.push_back(generate_item("wp", "Sword", "", ItemKind::Weapon, ItemSlot::Hand,
                                 ItemRarity::Common, 2, false));
    CHECK(!consume_item(p3, inv4, "wp")); // weapons are not consumable

    // Character factory.
    Attributes wa = starting_attributes(CharacterClass::Warrior);
    CHECK(wa.strength == 8 && wa.constitution == 7 && wa.intelligence == 3 && wa.stealth == 3);
    Attributes ma = starting_attributes(CharacterClass::Mage);
    CHECK(ma.intelligence == 8 && ma.wisdom == 7);

    Player hero = make_character("Hero", CharacterClass::Mage, "a scholar");
    CHECK(hero.name == "Hero" && hero.cls == CharacterClass::Mage);
    CHECK(hero.level == 1 && hero.gold == 50 && hero.hp == 50 && hero.max_hp == 50 &&
          hero.energy == 20);
    CHECK(hero.attributes.intelligence == 8);
    CHECK(hero.background == "a scholar");

    std::vector<Item> kit = starting_kit();
    CHECK(kit.size() == 5u);
    CHECK(kit[0].effects.strength == 3);       // Rusty Sword (2 + 1)
    CHECK(kit[1].effects.constitution == 2);   // Worn Leather Armor (1 + 1)
    CHECK(kit[2].effects.hp == 35 && kit[3].effects.hp == 35); // Minor Health Potions (20 + 15)
    CHECK(kit[4].effects.hp == 20);            // Stale Bread (5 + 15)

    CharacterClass c;
    CHECK(class_from_string("rogue", c) && c == CharacterClass::Rogue);
    CHECK(std::string(class_to_string(CharacterClass::Rogue)) == "rogue");
    CHECK(!class_from_string("paladin", c));

    if (failures == 0) {
        printf("items: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "items: %d checks failed\n", failures);
    return 1;
}
