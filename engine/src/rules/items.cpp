#include "oce/rules/items.hpp"

#include <algorithm>

namespace oce {

int weapon_armor_rarity_bonus(ItemRarity rarity) {
    switch (rarity) {
        case ItemRarity::Common:
            return 1;
        case ItemRarity::Uncommon:
            return 2;
        case ItemRarity::Rare:
            return 3;
        case ItemRarity::Epic:
            return 4;
        case ItemRarity::Legendary:
            return 5;
    }
    return 1;
}

int potion_rarity_bonus(ItemRarity rarity) {
    switch (rarity) {
        case ItemRarity::Common:
            return 15;
        case ItemRarity::Uncommon:
            return 30;
        case ItemRarity::Rare:
            return 50;
        case ItemRarity::Epic:
            return 75;
        case ItemRarity::Legendary:
            return 100;
    }
    return 15;
}

Item generate_item(const std::string& id, const std::string& name, const std::string& description,
                   ItemKind kind, ItemSlot slot, ItemRarity rarity, int base_power,
                   bool restores_energy) {
    Item item;
    item.id = id;
    item.name = name;
    item.description = description;
    item.kind = kind;
    item.slot = slot;
    item.rarity = rarity;
    switch (kind) {
        case ItemKind::Weapon:
            item.effects.strength = base_power + weapon_armor_rarity_bonus(rarity);
            break;
        case ItemKind::Armor:
            item.effects.constitution = base_power + weapon_armor_rarity_bonus(rarity);
            break;
        case ItemKind::Potion:
            if (restores_energy) {
                item.effects.energy = base_power + potion_rarity_bonus(rarity);
            } else {
                item.effects.hp = base_power + potion_rarity_bonus(rarity);
            }
            break;
    }
    return item;
}

namespace {

// Procedural-generation catalog: archetypes and rarity-derived name prefixes.
struct ItemArchetype {
    const char* noun;
    ItemKind kind;
    ItemSlot slot;
    int base_power;
    bool restores_energy;
};

const ItemArchetype kArchetypes[] = {
    {"Sword", ItemKind::Weapon, ItemSlot::Hand, 3, false},
    {"Axe", ItemKind::Weapon, ItemSlot::Hand, 4, false},
    {"Dagger", ItemKind::Weapon, ItemSlot::Hand, 2, false},
    {"Mace", ItemKind::Weapon, ItemSlot::Hand, 3, false},
    {"Spear", ItemKind::Weapon, ItemSlot::Hand, 3, false},
    {"Longbow", ItemKind::Weapon, ItemSlot::Hand, 3, false},
    {"Battle Staff", ItemKind::Weapon, ItemSlot::Hand, 2, false},
    {"Leather Armor", ItemKind::Armor, ItemSlot::Body, 2, false},
    {"Chainmail", ItemKind::Armor, ItemSlot::Body, 3, false},
    {"Plate Armor", ItemKind::Armor, ItemSlot::Body, 4, false},
    {"Padded Robe", ItemKind::Armor, ItemSlot::Body, 1, false},
    {"Tower Shield", ItemKind::Armor, ItemSlot::Body, 2, false},
    {"Health Potion", ItemKind::Potion, ItemSlot::Consumable, 20, false},
    {"Energy Draught", ItemKind::Potion, ItemSlot::Consumable, 15, true},
};

// Indexed by ItemRarity (Common .. Legendary).
const char* const kRarityPrefix[] = {"Worn", "Sturdy", "Fine", "Exquisite", "Fabled"};

} // namespace

ItemRarity roll_item_rarity(Rng& rng, int level) {
    const int roll = rng.between(1, 100) + (level > 0 ? level * 2 : 0);
    if (roll >= 98) {
        return ItemRarity::Legendary;
    }
    if (roll >= 90) {
        return ItemRarity::Epic;
    }
    if (roll >= 72) {
        return ItemRarity::Rare;
    }
    if (roll >= 45) {
        return ItemRarity::Uncommon;
    }
    return ItemRarity::Common;
}

Item random_item(Rng& rng, int level, ItemRarity rarity) {
    const int count = static_cast<int>(sizeof kArchetypes / sizeof kArchetypes[0]);
    const ItemArchetype& a = kArchetypes[rng.between(0, count - 1)];
    const std::string name = std::string(kRarityPrefix[static_cast<int>(rarity)]) + " " + a.noun;
    const std::string id = "item-" + std::to_string(rng.between(100000, 999999));
    const int power = a.base_power + (level > 0 ? level / 3 : 0);
    return generate_item(id, name, "A find of uncertain provenance.", a.kind, a.slot, rarity, power,
                         a.restores_energy);
}

Item random_item(Rng& rng, int level) {
    const ItemRarity rarity = roll_item_rarity(rng, level);
    return random_item(rng, level, rarity);
}

namespace {

std::vector<Item>::iterator find_by_id(std::vector<Item>& inventory, const std::string& id) {
    return std::find_if(inventory.begin(), inventory.end(),
                        [&id](const Item& it) { return it.id == id; });
}

} // namespace

bool equip_item(std::vector<Item>& inventory, Equipment& equipment, const std::string& item_id) {
    auto it = find_by_id(inventory, item_id);
    if (it == inventory.end()) {
        return false;
    }
    Item item = *it;

    std::optional<Item>* target = nullptr;
    if (item.kind == ItemKind::Weapon && item.slot == ItemSlot::Hand) {
        target = &equipment.hand;
    } else if (item.kind == ItemKind::Armor && item.slot == ItemSlot::Body) {
        target = &equipment.body;
    } else {
        return false; // not an equippable item
    }

    inventory.erase(it);
    if (target->has_value()) {
        inventory.push_back(**target);
    }
    *target = item;
    return true;
}

bool unequip_slot(std::vector<Item>& inventory, Equipment& equipment, ItemSlot slot) {
    std::optional<Item>* source = nullptr;
    if (slot == ItemSlot::Hand) {
        source = &equipment.hand;
    } else if (slot == ItemSlot::Body) {
        source = &equipment.body;
    }
    if (source == nullptr || !source->has_value()) {
        return false;
    }
    inventory.push_back(**source);
    source->reset();
    return true;
}

bool consume_item(Player& player, std::vector<Item>& inventory, const std::string& item_id) {
    auto it = find_by_id(inventory, item_id);
    if (it == inventory.end() || it->kind != ItemKind::Potion) {
        return false;
    }
    player.hp = std::min(player.max_hp, player.hp + it->effects.hp);
    player.energy = std::min(player.max_energy, player.energy + it->effects.energy);
    inventory.erase(it);
    return true;
}

} // namespace oce
