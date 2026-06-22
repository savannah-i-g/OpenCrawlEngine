#pragma once
// Item generation and inventory operations.

#include "oce/model.hpp"
#include "oce/rules/dice.hpp"

#include <string>
#include <vector>

namespace oce {

int weapon_armor_rarity_bonus(ItemRarity rarity); // 1..5
int potion_rarity_bonus(ItemRarity rarity);        // 15..100

// Builds an item with effects derived from kind, rarity, and base_power:
//   weapon -> strength = base_power + rarity bonus (1..5)
//   armor  -> constitution = base_power + rarity bonus (1..5)
//   potion -> hp (or energy, if restores_energy) = base_power + rarity bonus (15..100)
Item generate_item(const std::string& id, const std::string& name, const std::string& description,
                   ItemKind kind, ItemSlot slot, ItemRarity rarity, int base_power,
                   bool restores_energy);

// Rolls an item rarity, biased upward by level (higher levels find better gear).
ItemRarity roll_item_rarity(Rng& rng, int level);

// Procedurally generates an item: a random archetype (weapon, armor, or potion)
// named with a rarity-derived prefix, its power scaled by level. The first form
// rolls the rarity (level-biased); the second forces it. Ids are random.
Item random_item(Rng& rng, int level);
Item random_item(Rng& rng, int level, ItemRarity rarity);

// Equips an inventory item into its matching slot, returning any previously
// equipped item to the inventory. False if the id is absent or not equippable.
bool equip_item(std::vector<Item>& inventory, Equipment& equipment, const std::string& item_id);
// Returns an equipped item to the inventory. False if the slot is empty.
bool unequip_slot(std::vector<Item>& inventory, Equipment& equipment, ItemSlot slot);
// Consumes a potion, applying its hp/energy restoration (clamped to maxima) and
// removing it. False if the id is absent or the item is not a potion.
bool consume_item(Player& player, std::vector<Item>& inventory, const std::string& item_id);

} // namespace oce
