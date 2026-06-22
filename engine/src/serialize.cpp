#include "oce/serialize.hpp"

#include "oce/rules/character.hpp"

#include "oce_json.h"

#include <cstdlib>
#include <cstring>

namespace oce {
namespace {

// --- enum <-> string ------------------------------------------------------

const char* kind_str(ItemKind k) {
    switch (k) {
        case ItemKind::Weapon: return "weapon";
        case ItemKind::Armor: return "armor";
        case ItemKind::Potion: return "potion";
    }
    return "weapon";
}
ItemKind kind_from(const std::string& s) {
    if (s == "armor") return ItemKind::Armor;
    if (s == "potion") return ItemKind::Potion;
    return ItemKind::Weapon;
}

const char* rarity_str(ItemRarity r) {
    switch (r) {
        case ItemRarity::Common: return "common";
        case ItemRarity::Uncommon: return "uncommon";
        case ItemRarity::Rare: return "rare";
        case ItemRarity::Epic: return "epic";
        case ItemRarity::Legendary: return "legendary";
    }
    return "common";
}
ItemRarity rarity_from(const std::string& s) {
    if (s == "uncommon") return ItemRarity::Uncommon;
    if (s == "rare") return ItemRarity::Rare;
    if (s == "epic") return ItemRarity::Epic;
    if (s == "legendary") return ItemRarity::Legendary;
    return ItemRarity::Common;
}

const char* slot_str(ItemSlot s) {
    switch (s) {
        case ItemSlot::Hand: return "hand";
        case ItemSlot::Body: return "body";
        case ItemSlot::Consumable: return "consumable";
    }
    return "consumable";
}
ItemSlot slot_from(const std::string& s) {
    if (s == "hand") return ItemSlot::Hand;
    if (s == "body") return ItemSlot::Body;
    return ItemSlot::Consumable;
}

const char* business_str(BusinessType t) {
    switch (t) {
        case BusinessType::Tavern: return "tavern";
        case BusinessType::Shop: return "shop";
        case BusinessType::Farm: return "farm";
        case BusinessType::Mine: return "mine";
        case BusinessType::TradingCompany: return "trading_company";
        case BusinessType::MercenaryGuild: return "mercenary_guild";
        case BusinessType::Workshop: return "workshop";
        case BusinessType::Other: return "other";
    }
    return "other";
}
BusinessType business_from(const std::string& s) {
    if (s == "tavern") return BusinessType::Tavern;
    if (s == "shop") return BusinessType::Shop;
    if (s == "farm") return BusinessType::Farm;
    if (s == "mine") return BusinessType::Mine;
    if (s == "trading_company") return BusinessType::TradingCompany;
    if (s == "mercenary_guild") return BusinessType::MercenaryGuild;
    if (s == "workshop") return BusinessType::Workshop;
    return BusinessType::Other;
}

const char* faction_str(FactionType t) {
    switch (t) {
        case FactionType::Guild: return "guild";
        case FactionType::Kingdom: return "kingdom";
        case FactionType::Clan: return "clan";
        case FactionType::Cult: return "cult";
        case FactionType::MerchantCompany: return "merchant_company";
        case FactionType::Military: return "military";
        case FactionType::Religious: return "religious";
        case FactionType::Criminal: return "criminal";
        case FactionType::Other: return "other";
    }
    return "other";
}
FactionType faction_from(const std::string& s) {
    if (s == "guild") return FactionType::Guild;
    if (s == "kingdom") return FactionType::Kingdom;
    if (s == "clan") return FactionType::Clan;
    if (s == "cult") return FactionType::Cult;
    if (s == "merchant_company") return FactionType::MerchantCompany;
    if (s == "military") return FactionType::Military;
    if (s == "religious") return FactionType::Religious;
    if (s == "criminal") return FactionType::Criminal;
    return FactionType::Other;
}

// --- small helpers --------------------------------------------------------

oce_json* str_array(const std::vector<std::string>& v) {
    oce_json* a = oce_json_new_array();
    for (const std::string& s : v) {
        oce_json_arr_append_str(a, s.c_str());
    }
    return a;
}
void str_array_from(const oce_json* obj, const char* key, std::vector<std::string>& out) {
    const oce_json* a = oce_json_get(obj, key);
    if (!oce_json_is_array(a)) {
        return;
    }
    const size_t n = oce_json_arr_len(a);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(oce_json_as_str(oce_json_arr_at(a, i), ""));
    }
}

// --- per-struct (de)serialization ----------------------------------------

oce_json* attributes_to_json(const Attributes& a) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_int(o, "strength", a.strength);
    oce_json_obj_set_int(o, "dexterity", a.dexterity);
    oce_json_obj_set_int(o, "intelligence", a.intelligence);
    oce_json_obj_set_int(o, "constitution", a.constitution);
    oce_json_obj_set_int(o, "wisdom", a.wisdom);
    oce_json_obj_set_int(o, "charisma", a.charisma);
    oce_json_obj_set_int(o, "luck", a.luck);
    oce_json_obj_set_int(o, "perception", a.perception);
    oce_json_obj_set_int(o, "stealth", a.stealth);
    oce_json_obj_set_int(o, "bartering", a.bartering);
    return o;
}
void attributes_from_json(const oce_json* o, Attributes& a) {
    if (!oce_json_is_object(o)) return;
    a.strength = (int) oce_json_get_int(o, "strength", a.strength);
    a.dexterity = (int) oce_json_get_int(o, "dexterity", a.dexterity);
    a.intelligence = (int) oce_json_get_int(o, "intelligence", a.intelligence);
    a.constitution = (int) oce_json_get_int(o, "constitution", a.constitution);
    a.wisdom = (int) oce_json_get_int(o, "wisdom", a.wisdom);
    a.charisma = (int) oce_json_get_int(o, "charisma", a.charisma);
    a.luck = (int) oce_json_get_int(o, "luck", a.luck);
    a.perception = (int) oce_json_get_int(o, "perception", a.perception);
    a.stealth = (int) oce_json_get_int(o, "stealth", a.stealth);
    a.bartering = (int) oce_json_get_int(o, "bartering", a.bartering);
}

oce_json* effects_to_json(const ItemEffects& e) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_int(o, "strength", e.strength);
    oce_json_obj_set_int(o, "dexterity", e.dexterity);
    oce_json_obj_set_int(o, "intelligence", e.intelligence);
    oce_json_obj_set_int(o, "constitution", e.constitution);
    oce_json_obj_set_int(o, "wisdom", e.wisdom);
    oce_json_obj_set_int(o, "charisma", e.charisma);
    oce_json_obj_set_int(o, "luck", e.luck);
    oce_json_obj_set_int(o, "perception", e.perception);
    oce_json_obj_set_int(o, "stealth", e.stealth);
    oce_json_obj_set_int(o, "bartering", e.bartering);
    oce_json_obj_set_int(o, "hp", e.hp);
    oce_json_obj_set_int(o, "energy", e.energy);
    return o;
}
void effects_from_json(const oce_json* o, ItemEffects& e) {
    if (!oce_json_is_object(o)) return;
    e.strength = (int) oce_json_get_int(o, "strength", 0);
    e.dexterity = (int) oce_json_get_int(o, "dexterity", 0);
    e.intelligence = (int) oce_json_get_int(o, "intelligence", 0);
    e.constitution = (int) oce_json_get_int(o, "constitution", 0);
    e.wisdom = (int) oce_json_get_int(o, "wisdom", 0);
    e.charisma = (int) oce_json_get_int(o, "charisma", 0);
    e.luck = (int) oce_json_get_int(o, "luck", 0);
    e.perception = (int) oce_json_get_int(o, "perception", 0);
    e.stealth = (int) oce_json_get_int(o, "stealth", 0);
    e.bartering = (int) oce_json_get_int(o, "bartering", 0);
    e.hp = (int) oce_json_get_int(o, "hp", 0);
    e.energy = (int) oce_json_get_int(o, "energy", 0);
}

oce_json* item_to_json(const Item& it) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", it.id.c_str());
    oce_json_obj_set_str(o, "name", it.name.c_str());
    oce_json_obj_set_str(o, "description", it.description.c_str());
    oce_json_obj_set_str(o, "kind", kind_str(it.kind));
    oce_json_obj_set_str(o, "rarity", rarity_str(it.rarity));
    oce_json_obj_set_str(o, "slot", slot_str(it.slot));
    oce_json_obj_set(o, "effects", effects_to_json(it.effects));
    oce_json_obj_set_str(o, "icon", it.icon.c_str());
    return o;
}
Item item_from_json(const oce_json* o) {
    Item it;
    it.id = oce_json_get_str(o, "id", "");
    it.name = oce_json_get_str(o, "name", "");
    it.description = oce_json_get_str(o, "description", "");
    it.kind = kind_from(oce_json_get_str(o, "kind", "weapon"));
    it.rarity = rarity_from(oce_json_get_str(o, "rarity", "common"));
    it.slot = slot_from(oce_json_get_str(o, "slot", "consumable"));
    effects_from_json(oce_json_get(o, "effects"), it.effects);
    it.icon = oce_json_get_str(o, "icon", "");
    return it;
}

oce_json* player_to_json(const Player& p) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "name", p.name.c_str());
    oce_json_obj_set_str(o, "class", class_to_string(p.cls));
    oce_json_obj_set_int(o, "level", p.level);
    oce_json_obj_set_int(o, "xp", p.xp);
    oce_json_obj_set_int(o, "gold", p.gold);
    oce_json_obj_set_int(o, "hp", p.hp);
    oce_json_obj_set_int(o, "max_hp", p.max_hp);
    oce_json_obj_set_int(o, "energy", p.energy);
    oce_json_obj_set_int(o, "max_energy", p.max_energy);
    oce_json_obj_set_int(o, "attribute_points", p.attribute_points);
    oce_json_obj_set_str(o, "background", p.background.c_str());
    oce_json_obj_set(o, "attributes", attributes_to_json(p.attributes));
    return o;
}
void player_from_json(const oce_json* o, Player& p) {
    if (!oce_json_is_object(o)) return;
    p.name = oce_json_get_str(o, "name", "Adventurer");
    CharacterClass cls;
    if (class_from_string(oce_json_get_str(o, "class", "warrior"), cls)) {
        p.cls = cls;
    }
    p.level = (int) oce_json_get_int(o, "level", p.level);
    p.xp = oce_json_get_int(o, "xp", p.xp);
    p.gold = (int) oce_json_get_int(o, "gold", p.gold);
    p.hp = (int) oce_json_get_int(o, "hp", p.hp);
    p.max_hp = (int) oce_json_get_int(o, "max_hp", p.max_hp);
    p.energy = (int) oce_json_get_int(o, "energy", p.energy);
    p.max_energy = (int) oce_json_get_int(o, "max_energy", p.max_energy);
    p.attribute_points = (int) oce_json_get_int(o, "attribute_points", p.attribute_points);
    p.background = oce_json_get_str(o, "background", "");
    attributes_from_json(oce_json_get(o, "attributes"), p.attributes);
}

oce_json* enemy_to_json(const Enemy& e) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", e.id.c_str());
    oce_json_obj_set_str(o, "name", e.name.c_str());
    oce_json_obj_set_str(o, "description", e.description.c_str());
    oce_json_obj_set_int(o, "hp", e.hp);
    oce_json_obj_set_int(o, "max_hp", e.max_hp);
    oce_json_obj_set_int(o, "attack", e.attack);
    oce_json_obj_set_int(o, "defense", e.defense);
    return o;
}
Enemy enemy_from_json(const oce_json* o) {
    Enemy e;
    e.id = oce_json_get_str(o, "id", "");
    e.name = oce_json_get_str(o, "name", "");
    e.description = oce_json_get_str(o, "description", "");
    e.hp = (int) oce_json_get_int(o, "hp", 0);
    e.max_hp = (int) oce_json_get_int(o, "max_hp", 0);
    e.attack = (int) oce_json_get_int(o, "attack", 0);
    e.defense = (int) oce_json_get_int(o, "defense", 0);
    return e;
}

oce_json* business_to_json(const Business& b) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", b.id.c_str());
    oce_json_obj_set_str(o, "name", b.name.c_str());
    oce_json_obj_set_str(o, "type", business_str(b.type));
    oce_json_obj_set_str(o, "description", b.description.c_str());
    oce_json_obj_set_str(o, "location", b.location.c_str());
    oce_json_obj_set_int(o, "value", b.value);
    oce_json_obj_set_int(o, "income_per_day", b.income_per_day);
    oce_json_obj_set_int(o, "last_collected", b.last_collected);
    oce_json_obj_set_int(o, "employee_count", b.employee_count);
    oce_json_obj_set_str(o, "manager_id", b.manager_id.c_str());
    return o;
}
Business business_from_json(const oce_json* o) {
    Business b;
    b.id = oce_json_get_str(o, "id", "");
    b.name = oce_json_get_str(o, "name", "");
    b.type = business_from(oce_json_get_str(o, "type", "other"));
    b.description = oce_json_get_str(o, "description", "");
    b.location = oce_json_get_str(o, "location", "");
    b.value = (int) oce_json_get_int(o, "value", 0);
    b.income_per_day = (int) oce_json_get_int(o, "income_per_day", 0);
    b.last_collected = oce_json_get_int(o, "last_collected", 0);
    b.employee_count = (int) oce_json_get_int(o, "employee_count", 0);
    b.manager_id = oce_json_get_str(o, "manager_id", "");
    return b;
}

oce_json* relation_to_json(const Relation& r) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", r.id.c_str());
    oce_json_obj_set_str(o, "npc_id", r.npc_id.c_str());
    oce_json_obj_set_str(o, "npc_name", r.npc_name.c_str());
    oce_json_obj_set_str(o, "type", r.type.c_str());
    oce_json_obj_set_int(o, "strength", r.strength);
    oce_json_obj_set_str(o, "description", r.description.c_str());
    oce_json_obj_set(o, "benefits", str_array(r.benefits));
    return o;
}
Relation relation_from_json(const oce_json* o) {
    Relation r;
    r.id = oce_json_get_str(o, "id", "");
    r.npc_id = oce_json_get_str(o, "npc_id", "");
    r.npc_name = oce_json_get_str(o, "npc_name", "");
    r.type = oce_json_get_str(o, "type", "");
    r.strength = (int) oce_json_get_int(o, "strength", 0);
    r.description = oce_json_get_str(o, "description", "");
    str_array_from(o, "benefits", r.benefits);
    return r;
}

oce_json* property_to_json(const Property& p) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", p.id.c_str());
    oce_json_obj_set_str(o, "name", p.name.c_str());
    oce_json_obj_set_str(o, "type", p.type.c_str());
    oce_json_obj_set_str(o, "location", p.location.c_str());
    oce_json_obj_set_str(o, "description", p.description.c_str());
    oce_json_obj_set_int(o, "value", p.value);
    oce_json_obj_set(o, "provides", str_array(p.provides));
    return o;
}
Property property_from_json(const oce_json* o) {
    Property p;
    p.id = oce_json_get_str(o, "id", "");
    p.name = oce_json_get_str(o, "name", "");
    p.type = oce_json_get_str(o, "type", "");
    p.location = oce_json_get_str(o, "location", "");
    p.description = oce_json_get_str(o, "description", "");
    p.value = (int) oce_json_get_int(o, "value", 0);
    str_array_from(o, "provides", p.provides);
    return p;
}

oce_json* mount_to_json(const MountVehicle& m) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", m.id.c_str());
    oce_json_obj_set_str(o, "name", m.name.c_str());
    oce_json_obj_set_str(o, "type", m.type.c_str());
    oce_json_obj_set_str(o, "description", m.description.c_str());
    oce_json_obj_set_str(o, "era", m.era.c_str());
    oce_json_obj_set_double(o, "speed", m.speed);
    oce_json_obj_set_int(o, "capacity", m.capacity);
    oce_json_obj_set_int(o, "condition", m.condition);
    oce_json_obj_set_int(o, "upkeep_cost", m.upkeep_cost);
    oce_json_obj_set(o, "special_abilities", str_array(m.special_abilities));
    return o;
}
MountVehicle mount_from_json(const oce_json* o) {
    MountVehicle m;
    m.id = oce_json_get_str(o, "id", "");
    m.name = oce_json_get_str(o, "name", "");
    m.type = oce_json_get_str(o, "type", "");
    m.description = oce_json_get_str(o, "description", "");
    m.era = oce_json_get_str(o, "era", "");
    m.speed = oce_json_get_double(o, "speed", 1.0);
    m.capacity = (int) oce_json_get_int(o, "capacity", 0);
    m.condition = (int) oce_json_get_int(o, "condition", 100);
    m.upkeep_cost = (int) oce_json_get_int(o, "upkeep_cost", 0);
    str_array_from(o, "special_abilities", m.special_abilities);
    return m;
}

oce_json* npc_to_json(const NPC& n) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", n.id.c_str());
    oce_json_obj_set_str(o, "name", n.name.c_str());
    oce_json_obj_set_str(o, "description", n.description.c_str());
    oce_json_obj_set_str(o, "location", n.location.c_str());
    oce_json_obj_set_int(o, "relationship", n.relationship);
    oce_json_obj_set_str(o, "occupation", n.occupation.c_str());
    oce_json_obj_set_str(o, "last_dialogue", n.last_dialogue.c_str());
    return o;
}
NPC npc_from_json(const oce_json* o) {
    NPC n;
    n.id = oce_json_get_str(o, "id", "");
    n.name = oce_json_get_str(o, "name", "");
    n.description = oce_json_get_str(o, "description", "");
    n.location = oce_json_get_str(o, "location", "");
    n.relationship = (int) oce_json_get_int(o, "relationship", 0);
    n.occupation = oce_json_get_str(o, "occupation", "");
    n.last_dialogue = oce_json_get_str(o, "last_dialogue", "");
    return n;
}

oce_json* faction_to_json(const Faction& f) {
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "id", f.id.c_str());
    oce_json_obj_set_str(o, "name", f.name.c_str());
    oce_json_obj_set_str(o, "description", f.description.c_str());
    oce_json_obj_set_str(o, "type", faction_str(f.type));
    oce_json_obj_set_int(o, "relationship", f.relationship);
    oce_json_obj_set_int(o, "reputation", f.reputation);
    oce_json_obj_set_str(o, "territory", f.territory.c_str());
    oce_json_obj_set_str(o, "leader", f.leader.c_str());
    oce_json_obj_set(o, "benefits", str_array(f.benefits));
    oce_json_obj_set_bool(o, "discovered", f.discovered);
    oce_json_obj_set_str(o, "last_interaction", f.last_interaction.c_str());
    return o;
}
Faction faction_from_json(const oce_json* o) {
    Faction f;
    f.id = oce_json_get_str(o, "id", "");
    f.name = oce_json_get_str(o, "name", "");
    f.description = oce_json_get_str(o, "description", "");
    f.type = faction_from(oce_json_get_str(o, "type", "other"));
    f.relationship = (int) oce_json_get_int(o, "relationship", 0);
    f.reputation = (int) oce_json_get_int(o, "reputation", 0);
    f.territory = oce_json_get_str(o, "territory", "");
    f.leader = oce_json_get_str(o, "leader", "");
    str_array_from(o, "benefits", f.benefits);
    f.discovered = oce_json_get_bool(o, "discovered", false);
    f.last_interaction = oce_json_get_str(o, "last_interaction", "");
    return f;
}

template <class T, class ToJson>
oce_json* vec_to_json(const std::vector<T>& v, ToJson to_json) {
    oce_json* a = oce_json_new_array();
    for (const T& item : v) {
        oce_json_arr_append(a, to_json(item));
    }
    return a;
}

} // namespace

std::string serialize_game_state(const GameState& s) {
    oce_json* root = oce_json_new_object();
    oce_json_obj_set_int(root, "version", 1);
    oce_json_obj_set(root, "player", player_to_json(s.player));
    oce_json_obj_set(root, "inventory", vec_to_json(s.inventory, item_to_json));

    oce_json* eq = oce_json_new_object();
    if (s.equipment.hand.has_value()) {
        oce_json_obj_set(eq, "hand", item_to_json(s.equipment.hand.value()));
    }
    if (s.equipment.body.has_value()) {
        oce_json_obj_set(eq, "body", item_to_json(s.equipment.body.value()));
    }
    oce_json_obj_set(root, "equipment", eq);

    oce_json* assets = oce_json_new_object();
    oce_json_obj_set(assets, "businesses", vec_to_json(s.assets.businesses, business_to_json));
    oce_json_obj_set(assets, "relations", vec_to_json(s.assets.relations, relation_to_json));
    oce_json_obj_set(assets, "properties", vec_to_json(s.assets.properties, property_to_json));
    oce_json_obj_set(assets, "mounts", vec_to_json(s.assets.mounts, mount_to_json));
    oce_json_obj_set(root, "assets", assets);

    oce_json* story = oce_json_new_array();
    for (const Message& m : s.story) {
        oce_json* mo = oce_json_new_object();
        oce_json_obj_set_str(mo, "sender", m.sender.c_str());
        oce_json_obj_set_str(mo, "content", m.content.c_str());
        oce_json_obj_set_int(mo, "ts", m.ts);
        oce_json_arr_append(story, mo);
    }
    oce_json_obj_set(root, "story", story);

    oce_json_obj_set_str(root, "world_description", s.world_description.c_str());
    oce_json_obj_set_str(root, "world_context", s.world_context.c_str());

    oce_json* world = oce_json_new_object();
    oce_json_obj_set_str(world, "current_location", s.world_state.current_location.c_str());
    oce_json_obj_set(world, "visited_locations", str_array(s.world_state.visited_locations));
    oce_json_obj_set(world, "world_facts", str_array(s.world_state.world_facts));
    oce_json_obj_set_int(world, "time_elapsed", s.world_state.time_elapsed);
    oce_json* npcs = oce_json_new_array();
    for (const auto& kv : s.world_state.known_npcs) {
        oce_json_arr_append(npcs, npc_to_json(kv.second));
    }
    oce_json_obj_set(world, "known_npcs", npcs);
    oce_json* factions = oce_json_new_array();
    for (const auto& kv : s.world_state.factions) {
        oce_json_arr_append(factions, faction_to_json(kv.second));
    }
    oce_json_obj_set(world, "factions", factions);
    oce_json_obj_set(root, "world_state", world);

    oce_json* combat = oce_json_new_object();
    oce_json_obj_set_bool(combat, "active", s.combat.active);
    oce_json_obj_set_str(combat, "turn", s.combat.turn.c_str());
    oce_json_obj_set(combat, "enemies", vec_to_json(s.combat.enemies, enemy_to_json));
    oce_json_obj_set(combat, "log", str_array(s.combat.log));
    oce_json_obj_set(root, "combat", combat);

    oce_json* sc = oce_json_new_object();
    oce_json_obj_set_bool(sc, "active", s.skill_check.active);
    oce_json_obj_set_str(sc, "attribute", s.skill_check.attribute.c_str());
    oce_json_obj_set_int(sc, "difficulty", s.skill_check.difficulty);
    oce_json_obj_set_int(sc, "num_dice", s.skill_check.num_dice);
    oce_json_obj_set_str(sc, "description", s.skill_check.description.c_str());
    oce_json_obj_set_str(sc, "on_success", s.skill_check.on_success.c_str());
    oce_json_obj_set_str(sc, "on_failure", s.skill_check.on_failure.c_str());
    oce_json_obj_set(root, "skill_check", sc);

    oce_json_obj_set(root, "suggested_actions", str_array(s.suggested_actions));

    char* text = oce_json_print(root, false);
    std::string out = text ? text : "{}";
    free(text);
    oce_json_free(root);
    return out;
}

void deserialize_game_state(const char* json, GameState& out) {
    if (json == nullptr) {
        return;
    }
    oce_json* root = oce_json_parse(json, std::strlen(json));
    if (root == nullptr) {
        return;
    }

    player_from_json(oce_json_get(root, "player"), out.player);

    const oce_json* inv = oce_json_get(root, "inventory");
    if (oce_json_is_array(inv)) {
        out.inventory.clear();
        const size_t n = oce_json_arr_len(inv);
        for (size_t i = 0; i < n; ++i) {
            out.inventory.push_back(item_from_json(oce_json_arr_at(inv, i)));
        }
    }

    const oce_json* eq = oce_json_get(root, "equipment");
    if (oce_json_is_object(eq)) {
        const oce_json* hand = oce_json_get(eq, "hand");
        if (oce_json_is_object(hand)) {
            out.equipment.hand = item_from_json(hand);
        }
        const oce_json* body = oce_json_get(eq, "body");
        if (oce_json_is_object(body)) {
            out.equipment.body = item_from_json(body);
        }
    }

    const oce_json* assets = oce_json_get(root, "assets");
    if (oce_json_is_object(assets)) {
        const oce_json* b = oce_json_get(assets, "businesses");
        if (oce_json_is_array(b)) {
            const size_t n = oce_json_arr_len(b);
            for (size_t i = 0; i < n; ++i) {
                out.assets.businesses.push_back(business_from_json(oce_json_arr_at(b, i)));
            }
        }
        const oce_json* r = oce_json_get(assets, "relations");
        if (oce_json_is_array(r)) {
            const size_t n = oce_json_arr_len(r);
            for (size_t i = 0; i < n; ++i) {
                out.assets.relations.push_back(relation_from_json(oce_json_arr_at(r, i)));
            }
        }
        const oce_json* p = oce_json_get(assets, "properties");
        if (oce_json_is_array(p)) {
            const size_t n = oce_json_arr_len(p);
            for (size_t i = 0; i < n; ++i) {
                out.assets.properties.push_back(property_from_json(oce_json_arr_at(p, i)));
            }
        }
        const oce_json* m = oce_json_get(assets, "mounts");
        if (oce_json_is_array(m)) {
            const size_t n = oce_json_arr_len(m);
            for (size_t i = 0; i < n; ++i) {
                out.assets.mounts.push_back(mount_from_json(oce_json_arr_at(m, i)));
            }
        }
    }

    const oce_json* story = oce_json_get(root, "story");
    if (oce_json_is_array(story)) {
        out.story.clear();
        const size_t n = oce_json_arr_len(story);
        for (size_t i = 0; i < n; ++i) {
            const oce_json* m = oce_json_arr_at(story, i);
            Message msg;
            msg.sender = oce_json_get_str(m, "sender", "narrator");
            msg.content = oce_json_get_str(m, "content", "");
            msg.ts = oce_json_get_int(m, "ts", 0);
            out.story.push_back(std::move(msg));
        }
    }

    out.world_description = oce_json_get_str(root, "world_description", "");
    out.world_context = oce_json_get_str(root, "world_context", "");

    const oce_json* world = oce_json_get(root, "world_state");
    if (oce_json_is_object(world)) {
        out.world_state.current_location = oce_json_get_str(world, "current_location", "Unknown");
        str_array_from(world, "visited_locations", out.world_state.visited_locations);
        str_array_from(world, "world_facts", out.world_state.world_facts);
        out.world_state.time_elapsed = oce_json_get_int(world, "time_elapsed", 0);
        const oce_json* npcs = oce_json_get(world, "known_npcs");
        if (oce_json_is_array(npcs)) {
            const size_t n = oce_json_arr_len(npcs);
            for (size_t i = 0; i < n; ++i) {
                NPC npc = npc_from_json(oce_json_arr_at(npcs, i));
                out.world_state.known_npcs[npc.id] = npc;
            }
        }
        const oce_json* factions = oce_json_get(world, "factions");
        if (oce_json_is_array(factions)) {
            const size_t n = oce_json_arr_len(factions);
            for (size_t i = 0; i < n; ++i) {
                Faction f = faction_from_json(oce_json_arr_at(factions, i));
                out.world_state.factions[f.id] = f;
            }
        }
    }

    const oce_json* combat = oce_json_get(root, "combat");
    if (oce_json_is_object(combat)) {
        out.combat.active = oce_json_get_bool(combat, "active", false);
        out.combat.turn = oce_json_get_str(combat, "turn", "player");
        str_array_from(combat, "log", out.combat.log);
        const oce_json* en = oce_json_get(combat, "enemies");
        if (oce_json_is_array(en)) {
            const size_t n = oce_json_arr_len(en);
            for (size_t i = 0; i < n; ++i) {
                out.combat.enemies.push_back(enemy_from_json(oce_json_arr_at(en, i)));
            }
        }
    }

    const oce_json* sc = oce_json_get(root, "skill_check");
    if (oce_json_is_object(sc)) {
        out.skill_check.active = oce_json_get_bool(sc, "active", false);
        out.skill_check.attribute = oce_json_get_str(sc, "attribute", "strength");
        out.skill_check.difficulty = (int) oce_json_get_int(sc, "difficulty", 10);
        out.skill_check.num_dice = (int) oce_json_get_int(sc, "num_dice", 2);
        out.skill_check.description = oce_json_get_str(sc, "description", "");
        out.skill_check.on_success = oce_json_get_str(sc, "on_success", "");
        out.skill_check.on_failure = oce_json_get_str(sc, "on_failure", "");
    }

    str_array_from(root, "suggested_actions", out.suggested_actions);

    oce_json_free(root);
}

} // namespace oce
