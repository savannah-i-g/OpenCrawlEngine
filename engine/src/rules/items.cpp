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
