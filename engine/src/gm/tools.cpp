#include "oce/gm/tools.hpp"

#include "oce/rules/combat.hpp"
#include "oce/rules/items.hpp"
#include "oce/rules/leveling.hpp"
#include "oce/rules/world.hpp"

#include "oce_json.h"

#include <algorithm>
#include <string>

namespace oce {
namespace {

const char* const kOk = "{\"ok\":true}";

// `message` is always a controlled literal here, so no escaping is required.
std::string err(const char* message) {
    return std::string("{\"ok\":false,\"error\":\"") + message + "\"}";
}

std::string random_id(Rng& rng, const char* prefix) {
    return std::string(prefix) + "-" + std::to_string(rng.between(100000, 999999));
}

void read_string_array(const oce_json* obj, const char* key, std::vector<std::string>& out) {
    const oce_json* arr = oce_json_get(obj, key);
    if (!oce_json_is_array(arr)) {
        return;
    }
    const size_t n = oce_json_arr_len(arr);
    for (size_t i = 0; i < n && i < 16; ++i) {
        const char* s = oce_json_as_str(oce_json_arr_at(arr, i), "");
        if (s[0] != '\0') {
            out.emplace_back(s);
        }
    }
}

bool parse_kind(const std::string& s, ItemKind& out) {
    if (s == "weapon") {
        out = ItemKind::Weapon;
    } else if (s == "armor") {
        out = ItemKind::Armor;
    } else if (s == "potion") {
        out = ItemKind::Potion;
    } else {
        return false;
    }
    return true;
}

bool parse_rarity(const std::string& s, ItemRarity& out) {
    if (s == "common") {
        out = ItemRarity::Common;
    } else if (s == "uncommon") {
        out = ItemRarity::Uncommon;
    } else if (s == "rare") {
        out = ItemRarity::Rare;
    } else if (s == "epic") {
        out = ItemRarity::Epic;
    } else if (s == "legendary") {
        out = ItemRarity::Legendary;
    } else {
        return false;
    }
    return true;
}

bool parse_slot(const std::string& s, ItemSlot& out) {
    if (s == "hand") {
        out = ItemSlot::Hand;
    } else if (s == "body") {
        out = ItemSlot::Body;
    } else if (s == "consumable") {
        out = ItemSlot::Consumable;
    } else {
        return false;
    }
    return true;
}

ItemSlot default_slot_for(ItemKind kind) {
    switch (kind) {
        case ItemKind::Weapon:
            return ItemSlot::Hand;
        case ItemKind::Armor:
            return ItemSlot::Body;
        case ItemKind::Potion:
            return ItemSlot::Consumable;
    }
    return ItemSlot::Consumable;
}

BusinessType parse_business_type(const std::string& s) {
    if (s == "tavern") return BusinessType::Tavern;
    if (s == "shop") return BusinessType::Shop;
    if (s == "farm") return BusinessType::Farm;
    if (s == "mine") return BusinessType::Mine;
    if (s == "trading_company") return BusinessType::TradingCompany;
    if (s == "mercenary_guild") return BusinessType::MercenaryGuild;
    if (s == "workshop") return BusinessType::Workshop;
    return BusinessType::Other;
}

bool is_valid_attribute(const std::string& a) {
    return a == "strength" || a == "dexterity" || a == "intelligence" || a == "constitution" ||
           a == "wisdom" || a == "charisma" || a == "luck" || a == "perception" || a == "stealth" ||
           a == "bartering";
}

// Builds an item from a JSON object. Returns false with *err_msg set on bad input.
bool make_item_from_json(const oce_json* obj, Rng& rng, Item& out, const char** err_msg) {
    if (!oce_json_is_object(obj)) {
        *err_msg = "invalid item";
        return false;
    }
    ItemKind kind;
    if (!parse_kind(oce_json_get_str(obj, "type", ""), kind)) {
        *err_msg = "invalid item type";
        return false;
    }
    ItemRarity rarity;
    if (!parse_rarity(oce_json_get_str(obj, "rarity", "common"), rarity)) {
        *err_msg = "invalid item rarity";
        return false;
    }
    const std::string slot_s = oce_json_get_str(obj, "slot", "");
    ItemSlot slot = default_slot_for(kind);
    if (!slot_s.empty() && !parse_slot(slot_s, slot)) {
        *err_msg = "invalid item slot";
        return false;
    }
    int power = (int) oce_json_get_int(obj, "power", 1);
    power = std::clamp(power, 0, 1000);
    const bool restores_energy = oce_json_get_bool(obj, "restores_energy", false);

    out = generate_item(random_id(rng, "item"), oce_json_get_str(obj, "name", "Item"),
                        oce_json_get_str(obj, "description", ""), kind, slot, rarity, power,
                        restores_energy);
    out.icon = oce_json_get_str(obj, "icon", "");
    return true;
}

// --- player ---------------------------------------------------------------

std::string apply_stat_changes(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    Player& pl = s.player;
    pl.hp = std::clamp(pl.hp + (int) oce_json_get_int(args, "hp", 0), 0, pl.max_hp);
    pl.gold = std::max(0, pl.gold + (int) oce_json_get_int(args, "gold", 0));
    pl.energy = std::clamp(pl.energy + (int) oce_json_get_int(args, "energy", 0), 0, pl.max_energy);
    int levels = 0;
    const long long xp = oce_json_get_int(args, "xp", 0);
    if (xp > 0) {
        pl.xp += xp;
        levels = apply_level_up(pl);
    }
    std::string r = "{\"ok\":true,\"hp\":" + std::to_string(pl.hp) +
                    ",\"gold\":" + std::to_string(pl.gold) +
                    ",\"energy\":" + std::to_string(pl.energy) +
                    ",\"level\":" + std::to_string(pl.level);
    if (levels > 0) {
        r += ",\"leveled_up\":" + std::to_string(levels);
    }
    r += "}";
    return r;
}

std::string set_suggested_actions(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const oce_json* arr = oce_json_get(args, "actions");
    if (!oce_json_is_array(arr)) {
        return err("actions must be an array");
    }
    std::vector<std::string> actions;
    const size_t n = oce_json_arr_len(arr);
    for (size_t i = 0; i < n && i < 8; ++i) {
        const char* a = oce_json_as_str(oce_json_arr_at(arr, i), "");
        if (a[0] != '\0') {
            actions.emplace_back(a);
        }
    }
    s.suggested_actions = std::move(actions);
    return kOk;
}

// --- combat & skill checks ------------------------------------------------

std::string start_combat(GameState& s, const oce_json* args, Rng& rng) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const oce_json* arr = oce_json_get(args, "enemies");
    if (!oce_json_is_array(arr) || oce_json_arr_len(arr) == 0) {
        return err("enemies must be a non-empty array");
    }
    size_t n = oce_json_arr_len(arr);
    if (n > 8) {
        n = 8;
    }
    s.combat.enemies.clear();
    for (size_t i = 0; i < n; ++i) {
        const oce_json* e = oce_json_arr_at(arr, i);
        const int level = std::clamp((int) oce_json_get_int(e, "level", 1), 1, 100);
        s.combat.enemies.push_back(make_enemy(rng, level, "enemy-" + std::to_string(i),
                                              oce_json_get_str(e, "name", "Enemy"),
                                              oce_json_get_str(e, "description", "")));
    }
    s.combat.active = true;
    s.combat.turn = "player";
    s.combat.log.clear();
    return "{\"ok\":true,\"enemies\":" + std::to_string(s.combat.enemies.size()) + "}";
}

std::string end_combat(GameState& s, const oce_json* args, Rng& rng) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string outcome = oce_json_get_str(args, "outcome", "");
    if (outcome != "victory" && outcome != "defeat" && outcome != "fled") {
        return err("outcome must be victory, defeat, or fled");
    }
    s.combat.active = false;
    s.combat.enemies.clear();
    s.combat.turn = "player";
    s.combat.log.clear();

    int levels = 0;
    if (outcome == "victory") {
        const long long xp = oce_json_get_int(args, "xp", 0);
        if (xp > 0) {
            s.player.xp += xp;
            levels = apply_level_up(s.player);
        }
        const int gold = (int) oce_json_get_int(args, "gold", 0);
        if (gold > 0) {
            s.player.gold += gold;
        }
        const oce_json* loot = oce_json_get(args, "loot");
        if (oce_json_is_array(loot)) {
            const size_t n = oce_json_arr_len(loot);
            for (size_t i = 0; i < n && i < 16; ++i) {
                Item item;
                const char* emsg = nullptr;
                if (make_item_from_json(oce_json_arr_at(loot, i), rng, item, &emsg)) {
                    s.inventory.push_back(item);
                }
            }
        }
    }
    std::string r = "{\"ok\":true,\"outcome\":\"" + outcome + "\"";
    if (levels > 0) {
        r += ",\"leveled_up\":" + std::to_string(levels);
    }
    r += "}";
    return r;
}

std::string set_skill_check(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string attribute = oce_json_get_str(args, "attribute", "");
    if (!is_valid_attribute(attribute)) {
        return err("invalid attribute");
    }
    int num_dice = (int) oce_json_get_int(args, "num_dice", 2);
    if (num_dice != 2 && num_dice != 4) {
        num_dice = 2;
    }
    s.skill_check.active = true;
    s.skill_check.attribute = attribute;
    s.skill_check.difficulty = (int) oce_json_get_int(args, "difficulty", 10);
    s.skill_check.num_dice = num_dice;
    s.skill_check.description = oce_json_get_str(args, "description", "");
    s.skill_check.on_success = oce_json_get_str(args, "on_success", "");
    s.skill_check.on_failure = oce_json_get_str(args, "on_failure", "");
    return kOk;
}

// --- inventory ------------------------------------------------------------

std::string add_item(GameState& s, const oce_json* args, Rng& rng) {
    Item item;
    const char* emsg = "invalid arguments";
    if (!make_item_from_json(args, rng, item, &emsg)) {
        return err(emsg);
    }
    s.inventory.push_back(item);
    return "{\"ok\":true,\"item_id\":\"" + item.id + "\"}"; // id is "item-<digits>", safe to embed
}

std::string remove_item(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string id = oce_json_get_str(args, "item_id", "");
    auto it = std::find_if(s.inventory.begin(), s.inventory.end(),
                           [&id](const Item& i) { return i.id == id; });
    if (it == s.inventory.end()) {
        return err("item not found");
    }
    s.inventory.erase(it);
    return kOk;
}

std::string equip_item_tool(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    if (!equip_item(s.inventory, s.equipment, oce_json_get_str(args, "item_id", ""))) {
        return err("item not found or not equippable");
    }
    return kOk;
}

std::string unequip_item_tool(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string slot_s = oce_json_get_str(args, "slot", "");
    ItemSlot slot;
    if (slot_s == "hand") {
        slot = ItemSlot::Hand;
    } else if (slot_s == "body") {
        slot = ItemSlot::Body;
    } else {
        return err("slot must be hand or body");
    }
    if (!unequip_slot(s.inventory, s.equipment, slot)) {
        return err("nothing equipped in that slot");
    }
    return kOk;
}

// --- assets & factions ----------------------------------------------------

std::string add_business(GameState& s, const oce_json* args, Rng& rng) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    Business b;
    b.id = random_id(rng, "biz");
    b.name = oce_json_get_str(args, "name", "Business");
    b.type = parse_business_type(oce_json_get_str(args, "type", "other"));
    b.description = oce_json_get_str(args, "description", "");
    b.location = oce_json_get_str(args, "location", "");
    b.value = std::max(0, (int) oce_json_get_int(args, "value", 0));
    b.income_per_day = std::max(0, (int) oce_json_get_int(args, "income_per_day", 0));
    b.last_collected = s.world_state.time_elapsed;
    s.assets.businesses.push_back(b);
    return kOk;
}

std::string add_relation(GameState& s, const oce_json* args, Rng& rng) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    Relation r;
    r.id = random_id(rng, "rel");
    r.npc_id = oce_json_get_str(args, "npc_id", "");
    r.npc_name = oce_json_get_str(args, "npc_name", "");
    r.type = oce_json_get_str(args, "type", "friend");
    r.strength = std::clamp((int) oce_json_get_int(args, "strength", 0), -100, 100);
    r.description = oce_json_get_str(args, "description", "");
    read_string_array(args, "benefits", r.benefits);
    s.assets.relations.push_back(r);
    return kOk;
}

std::string add_property(GameState& s, const oce_json* args, Rng& rng) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    Property p;
    p.id = random_id(rng, "prop");
    p.name = oce_json_get_str(args, "name", "Property");
    p.type = oce_json_get_str(args, "type", "other");
    p.location = oce_json_get_str(args, "location", "");
    p.description = oce_json_get_str(args, "description", "");
    p.value = std::max(0, (int) oce_json_get_int(args, "value", 0));
    read_string_array(args, "provides", p.provides);
    s.assets.properties.push_back(p);
    return kOk;
}

std::string add_mount(GameState& s, const oce_json* args, Rng& rng) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    MountVehicle m;
    m.id = random_id(rng, "mount");
    m.name = oce_json_get_str(args, "name", "Mount");
    m.type = oce_json_get_str(args, "type", "horse");
    m.description = oce_json_get_str(args, "description", "");
    m.era = oce_json_get_str(args, "era", "medieval");
    m.speed = oce_json_get_double(args, "speed", 1.0);
    m.capacity = std::max(0, (int) oce_json_get_int(args, "capacity", 0));
    m.condition = 100;
    m.upkeep_cost = std::max(0, (int) oce_json_get_int(args, "upkeep_cost", 0));
    read_string_array(args, "special_abilities", m.special_abilities);
    s.assets.mounts.push_back(m);
    return kOk;
}

std::string change_faction_tool(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string id = oce_json_get_str(args, "faction_id", "");
    if (id.empty()) {
        return err("faction_id required");
    }
    Faction& f = s.world_state.factions[id]; // inserts if absent
    if (f.id.empty()) {
        f.id = id;
        f.discovered = true;
    }
    const std::string name = oce_json_get_str(args, "name", "");
    if (!name.empty()) {
        f.name = name;
    }
    change_faction(f, (int) oce_json_get_int(args, "relationship_change", 0),
                   (int) oce_json_get_int(args, "reputation_change", 0));
    return kOk;
}

// --- world ----------------------------------------------------------------

std::string upsert_npc_tool(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string id = oce_json_get_str(args, "id", "");
    if (id.empty()) {
        return err("id required");
    }
    NPC npc;
    npc.id = id;
    npc.name = oce_json_get_str(args, "name", "");
    npc.description = oce_json_get_str(args, "description", "");
    npc.location = oce_json_get_str(args, "location", "");
    npc.occupation = oce_json_get_str(args, "occupation", "");
    npc.relationship = std::clamp((int) oce_json_get_int(args, "relationship", 0), -100, 100);
    upsert_npc(s.world_state, npc);
    return kOk;
}

std::string set_location_tool(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string location = oce_json_get_str(args, "location", "");
    if (location.empty()) {
        return err("location required");
    }
    set_location(s.world_state, location);
    advance_time(s.world_state, oce_json_get_int(args, "time_elapsed_minutes", 0));
    return kOk;
}

std::string add_world_fact_tool(GameState& s, const oce_json* args, Rng&) {
    if (!oce_json_is_object(args)) {
        return err("invalid arguments");
    }
    const std::string fact = oce_json_get_str(args, "fact", "");
    if (fact.empty()) {
        return err("fact required");
    }
    add_world_fact(s.world_state, fact);
    return kOk;
}

// --- tool specs (OpenAI function schemas) ---------------------------------

const char* const kSpecApplyStat =
    "{\"type\":\"function\",\"function\":{\"name\":\"apply_stat_changes\","
    "\"description\":\"Apply signed deltas to the player's hp, energy, gold, and xp.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"hp\":{\"type\":\"integer\"},\"energy\":{\"type\":\"integer\"},"
    "\"gold\":{\"type\":\"integer\"},\"xp\":{\"type\":\"integer\"}}}}}";
const char* const kSpecSuggested =
    "{\"type\":\"function\",\"function\":{\"name\":\"set_suggested_actions\","
    "\"description\":\"Offer two to four short suggested next actions.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"actions\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[\"actions\"]}}}";
const char* const kSpecStartCombat =
    "{\"type\":\"function\",\"function\":{\"name\":\"start_combat\","
    "\"description\":\"Begin combat with one or more enemies; the engine derives their stats.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"enemies\":{\"type\":\"array\",\"items\":{"
    "\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
    "\"description\":{\"type\":\"string\"},\"level\":{\"type\":\"integer\"}},"
    "\"required\":[\"name\",\"level\"]}}},\"required\":[\"enemies\"]}}}";
const char* const kSpecEndCombat =
    "{\"type\":\"function\",\"function\":{\"name\":\"end_combat\","
    "\"description\":\"Resolve combat. On victory, optionally award xp, gold, and loot items.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"outcome\":{\"type\":\"string\",\"enum\":[\"victory\",\"defeat\",\"fled\"]},"
    "\"xp\":{\"type\":\"integer\"},\"gold\":{\"type\":\"integer\"},"
    "\"loot\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}},\"required\":[\"outcome\"]}}}";
const char* const kSpecSkillCheck =
    "{\"type\":\"function\",\"function\":{\"name\":\"set_skill_check\","
    "\"description\":\"Request a skill check before resolving a risky action.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"attribute\":{\"type\":\"string\"},\"difficulty\":{\"type\":\"integer\"},"
    "\"num_dice\":{\"type\":\"integer\",\"enum\":[2,4]},\"description\":{\"type\":\"string\"},"
    "\"on_success\":{\"type\":\"string\"},\"on_failure\":{\"type\":\"string\"}},"
    "\"required\":[\"attribute\",\"difficulty\"]}}}";
const char* const kSpecAddItem =
    "{\"type\":\"function\",\"function\":{\"name\":\"add_item\","
    "\"description\":\"Add an item to the inventory; the engine derives its effects from power.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
    "\"description\":{\"type\":\"string\"},\"type\":{\"type\":\"string\",\"enum\":[\"weapon\",\"armor\",\"potion\"]},"
    "\"rarity\":{\"type\":\"string\",\"enum\":[\"common\",\"uncommon\",\"rare\",\"epic\",\"legendary\"]},"
    "\"slot\":{\"type\":\"string\",\"enum\":[\"hand\",\"body\",\"consumable\"]},"
    "\"power\":{\"type\":\"integer\"},\"restores_energy\":{\"type\":\"boolean\"}},"
    "\"required\":[\"name\",\"type\"]}}}";
const char* const kSpecRemoveItem =
    "{\"type\":\"function\",\"function\":{\"name\":\"remove_item\","
    "\"description\":\"Remove an item from the inventory by id.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"item_id\":{\"type\":\"string\"}},"
    "\"required\":[\"item_id\"]}}}";
const char* const kSpecEquip =
    "{\"type\":\"function\",\"function\":{\"name\":\"equip_item\","
    "\"description\":\"Equip an inventory item into its slot.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"item_id\":{\"type\":\"string\"}},"
    "\"required\":[\"item_id\"]}}}";
const char* const kSpecUnequip =
    "{\"type\":\"function\",\"function\":{\"name\":\"unequip_item\","
    "\"description\":\"Unequip the item in the given slot.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"slot\":{\"type\":\"string\",\"enum\":[\"hand\",\"body\"]}},"
    "\"required\":[\"slot\"]}}}";
const char* const kSpecAddBusiness =
    "{\"type\":\"function\",\"function\":{\"name\":\"add_business\","
    "\"description\":\"Grant the player a business that yields daily gold.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
    "\"type\":{\"type\":\"string\"},\"description\":{\"type\":\"string\"},\"location\":{\"type\":\"string\"},"
    "\"value\":{\"type\":\"integer\"},\"income_per_day\":{\"type\":\"integer\"}},\"required\":[\"name\"]}}}";
const char* const kSpecAddRelation =
    "{\"type\":\"function\",\"function\":{\"name\":\"add_relation\","
    "\"description\":\"Record a relationship with a named NPC.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"npc_id\":{\"type\":\"string\"},"
    "\"npc_name\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"strength\":{\"type\":\"integer\"},"
    "\"description\":{\"type\":\"string\"},\"benefits\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},"
    "\"required\":[\"npc_name\"]}}}";
const char* const kSpecAddProperty =
    "{\"type\":\"function\",\"function\":{\"name\":\"add_property\","
    "\"description\":\"Grant the player a property.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
    "\"type\":{\"type\":\"string\"},\"location\":{\"type\":\"string\"},\"description\":{\"type\":\"string\"},"
    "\"value\":{\"type\":\"integer\"},\"provides\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},"
    "\"required\":[\"name\"]}}}";
const char* const kSpecAddMount =
    "{\"type\":\"function\",\"function\":{\"name\":\"add_mount\","
    "\"description\":\"Grant the player a mount or vehicle.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
    "\"type\":{\"type\":\"string\"},\"description\":{\"type\":\"string\"},\"era\":{\"type\":\"string\"},"
    "\"speed\":{\"type\":\"number\"},\"capacity\":{\"type\":\"integer\"},\"upkeep_cost\":{\"type\":\"integer\"},"
    "\"special_abilities\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[\"name\"]}}}";
const char* const kSpecChangeFaction =
    "{\"type\":\"function\",\"function\":{\"name\":\"change_faction\","
    "\"description\":\"Adjust the player's standing with a faction, creating it if new.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"faction_id\":{\"type\":\"string\"},"
    "\"name\":{\"type\":\"string\"},\"relationship_change\":{\"type\":\"integer\"},"
    "\"reputation_change\":{\"type\":\"integer\"}},\"required\":[\"faction_id\"]}}}";
const char* const kSpecUpsertNpc =
    "{\"type\":\"function\",\"function\":{\"name\":\"upsert_npc\","
    "\"description\":\"Record or update a known NPC.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},"
    "\"name\":{\"type\":\"string\"},\"description\":{\"type\":\"string\"},\"location\":{\"type\":\"string\"},"
    "\"occupation\":{\"type\":\"string\"},\"relationship\":{\"type\":\"integer\"}},\"required\":[\"id\",\"name\"]}}}";
const char* const kSpecSetLocation =
    "{\"type\":\"function\",\"function\":{\"name\":\"set_location\","
    "\"description\":\"Move the player to a location and optionally advance game time.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\"},"
    "\"time_elapsed_minutes\":{\"type\":\"integer\"}},\"required\":[\"location\"]}}}";
const char* const kSpecAddWorldFact =
    "{\"type\":\"function\",\"function\":{\"name\":\"add_world_fact\","
    "\"description\":\"Record a durable fact about the world.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"fact\":{\"type\":\"string\"}},"
    "\"required\":[\"fact\"]}}}";

} // namespace

const std::vector<GmTool>& gm_tools() {
    static const std::vector<GmTool> tools = {
        {"apply_stat_changes", kSpecApplyStat, apply_stat_changes},
        {"set_suggested_actions", kSpecSuggested, set_suggested_actions},
        {"start_combat", kSpecStartCombat, start_combat},
        {"end_combat", kSpecEndCombat, end_combat},
        {"set_skill_check", kSpecSkillCheck, set_skill_check},
        {"add_item", kSpecAddItem, add_item},
        {"remove_item", kSpecRemoveItem, remove_item},
        {"equip_item", kSpecEquip, equip_item_tool},
        {"unequip_item", kSpecUnequip, unequip_item_tool},
        {"add_business", kSpecAddBusiness, add_business},
        {"add_relation", kSpecAddRelation, add_relation},
        {"add_property", kSpecAddProperty, add_property},
        {"add_mount", kSpecAddMount, add_mount},
        {"change_faction", kSpecChangeFaction, change_faction_tool},
        {"upsert_npc", kSpecUpsertNpc, upsert_npc_tool},
        {"set_location", kSpecSetLocation, set_location_tool},
        {"add_world_fact", kSpecAddWorldFact, add_world_fact_tool},
    };
    return tools;
}

} // namespace oce
