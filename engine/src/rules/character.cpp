#include "oce/rules/character.hpp"

#include "oce/rules/items.hpp"

namespace oce {

Attributes starting_attributes(CharacterClass cls) {
    Attributes a;
    switch (cls) {
        case CharacterClass::Warrior:
            a = {8, 5, 3, 7, 4, 4, 4, 5, 3, 4};
            break;
        case CharacterClass::Rogue:
            a = {4, 8, 5, 4, 5, 6, 7, 6, 8, 5};
            break;
        case CharacterClass::Mage:
            a = {3, 4, 8, 4, 7, 5, 5, 6, 4, 4};
            break;
        case CharacterClass::Cleric:
            a = {5, 4, 5, 6, 8, 7, 5, 5, 3, 5};
            break;
        case CharacterClass::Ranger:
            a = {5, 7, 4, 6, 6, 4, 5, 8, 6, 4};
            break;
        case CharacterClass::Bard:
            a = {4, 6, 6, 5, 5, 8, 6, 5, 5, 8};
            break;
    }
    return a;
}

Player make_character(const std::string& name, CharacterClass cls, const std::string& background) {
    Player p;
    p.name = name;
    p.cls = cls;
    p.level = 1;
    p.xp = 0;
    p.gold = 50;
    p.hp = 50;
    p.max_hp = 50;
    p.energy = 20;
    p.max_energy = 20;
    p.attributes = starting_attributes(cls);
    p.attribute_points = 0;
    p.background = background;
    return p;
}

const char* class_to_string(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Warrior:
            return "warrior";
        case CharacterClass::Rogue:
            return "rogue";
        case CharacterClass::Mage:
            return "mage";
        case CharacterClass::Cleric:
            return "cleric";
        case CharacterClass::Ranger:
            return "ranger";
        case CharacterClass::Bard:
            return "bard";
    }
    return "warrior";
}

bool class_from_string(const std::string& s, CharacterClass& out) {
    if (s == "warrior") {
        out = CharacterClass::Warrior;
    } else if (s == "rogue") {
        out = CharacterClass::Rogue;
    } else if (s == "mage") {
        out = CharacterClass::Mage;
    } else if (s == "cleric") {
        out = CharacterClass::Cleric;
    } else if (s == "ranger") {
        out = CharacterClass::Ranger;
    } else if (s == "bard") {
        out = CharacterClass::Bard;
    } else {
        return false;
    }
    return true;
}

const char* difficulty_to_string(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:
            return "easy";
        case Difficulty::Normal:
            return "normal";
        case Difficulty::Hard:
            return "hard";
        case Difficulty::Deadly:
            return "deadly";
    }
    return "normal";
}

bool allocate_attribute(Player& player, const std::string& attribute) {
    if (player.attribute_points <= 0) {
        return false;
    }
    Attributes& a = player.attributes;
    int* slot = nullptr;
    if (attribute == "strength") {
        slot = &a.strength;
    } else if (attribute == "dexterity") {
        slot = &a.dexterity;
    } else if (attribute == "intelligence") {
        slot = &a.intelligence;
    } else if (attribute == "constitution") {
        slot = &a.constitution;
    } else if (attribute == "wisdom") {
        slot = &a.wisdom;
    } else if (attribute == "charisma") {
        slot = &a.charisma;
    } else if (attribute == "luck") {
        slot = &a.luck;
    } else if (attribute == "perception") {
        slot = &a.perception;
    } else if (attribute == "stealth") {
        slot = &a.stealth;
    } else if (attribute == "bartering") {
        slot = &a.bartering;
    } else {
        return false;
    }
    *slot += 1;
    player.attribute_points -= 1;
    return true;
}

bool difficulty_from_string(const std::string& s, Difficulty& out) {
    if (s == "easy") {
        out = Difficulty::Easy;
    } else if (s == "normal") {
        out = Difficulty::Normal;
    } else if (s == "hard") {
        out = Difficulty::Hard;
    } else if (s == "deadly") {
        out = Difficulty::Deadly;
    } else {
        return false;
    }
    return true;
}

} // namespace oce
