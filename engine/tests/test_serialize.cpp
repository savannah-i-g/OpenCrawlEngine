// Round-trips a populated GameState through JSON and back.
#include "oce/model.hpp"
#include "oce/serialize.hpp"

#include <cstdio>
#include <string>

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

    GameState s;
    s.player.name = "Hero";
    s.player.cls = CharacterClass::Mage;
    s.player.level = 3;
    s.player.gold = 120;
    s.player.hp = 42;
    s.player.attributes.intelligence = 8;

    Item sword;
    sword.id = "sw1";
    sword.name = "Blade";
    sword.kind = ItemKind::Weapon;
    sword.rarity = ItemRarity::Rare;
    sword.slot = ItemSlot::Hand;
    sword.effects.strength = 5;
    s.inventory.push_back(sword);

    Item mail;
    mail.id = "ar1";
    mail.name = "Mail";
    mail.kind = ItemKind::Armor;
    mail.slot = ItemSlot::Body;
    mail.effects.constitution = 3;
    s.equipment.body = mail;

    Business b;
    b.id = "b1";
    b.name = "Tavern";
    b.type = BusinessType::Tavern;
    b.income_per_day = 10;
    b.last_collected = 1440;
    s.assets.businesses.push_back(b);

    Relation r;
    r.id = "r1";
    r.npc_name = "Bob";
    r.type = "ally";
    r.strength = 50;
    r.benefits = {"discount"};
    s.assets.relations.push_back(r);

    s.world_state.current_location = "Town";
    s.world_state.time_elapsed = 2880;
    s.world_state.world_facts = {"It rains often."};
    NPC n;
    n.id = "n1";
    n.name = "Bob";
    n.location = "Town";
    s.world_state.known_npcs["n1"] = n;
    Faction f;
    f.id = "guild";
    f.name = "Guild";
    f.type = FactionType::Guild;
    f.relationship = 30;
    f.reputation = 200;
    f.discovered = true;
    s.world_state.factions["guild"] = f;

    s.story.push_back(Message{"narrator", "Once upon a time.", 5});
    s.suggested_actions = {"Go", "Stay"};

    const std::string json = serialize_game_state(s);
    CHECK(!json.empty());

    GameState t;
    deserialize_game_state(json.c_str(), t);

    CHECK(t.player.name == "Hero" && t.player.cls == CharacterClass::Mage);
    CHECK(t.player.level == 3 && t.player.gold == 120 && t.player.hp == 42);
    CHECK(t.player.attributes.intelligence == 8);

    CHECK(t.inventory.size() == 1u && t.inventory[0].id == "sw1");
    CHECK(t.inventory[0].effects.strength == 5 && t.inventory[0].rarity == ItemRarity::Rare);
    CHECK(t.equipment.body.has_value() && t.equipment.body->name == "Mail");
    CHECK(t.equipment.body->effects.constitution == 3);
    CHECK(!t.equipment.hand.has_value());

    CHECK(t.assets.businesses.size() == 1u);
    CHECK(t.assets.businesses[0].type == BusinessType::Tavern);
    CHECK(t.assets.businesses[0].income_per_day == 10 && t.assets.businesses[0].last_collected == 1440);
    CHECK(t.assets.relations.size() == 1u && t.assets.relations[0].strength == 50);
    CHECK(t.assets.relations[0].benefits.size() == 1u);

    CHECK(t.world_state.current_location == "Town" && t.world_state.time_elapsed == 2880);
    CHECK(t.world_state.world_facts.size() == 1u);
    CHECK(t.world_state.known_npcs.count("n1") == 1u && t.world_state.known_npcs["n1"].name == "Bob");
    CHECK(t.world_state.factions.count("guild") == 1u);
    CHECK(t.world_state.factions["guild"].reputation == 200 && t.world_state.factions["guild"].discovered);

    CHECK(t.story.size() == 1u && t.story[0].content == "Once upon a time." && t.story[0].ts == 5);
    CHECK(t.suggested_actions.size() == 2u);

    if (failures == 0) {
        printf("serialize: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "serialize: %d checks failed\n", failures);
    return 1;
}
