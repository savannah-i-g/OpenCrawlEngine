// Tests every game-master tool directly: happy paths plus robustness (malformed
// args, missing/bad ids, unknown enums, out-of-range values) with no state
// corruption. Tools are pure GameState mutations, so no engine is needed.
#include "oce/gm/tools.hpp"

#include "oce_json.h"

#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

static const oce::GmTool* find_tool(const char* name) {
    for (const oce::GmTool& t : oce::gm_tools()) {
        if (std::strcmp(t.name, name) == 0) {
            return &t;
        }
    }
    return nullptr;
}

static std::string call(const char* name, const char* args_json, oce::GameState& s, oce::Rng& rng) {
    const oce::GmTool* t = find_tool(name);
    if (t == nullptr) {
        return "{\"ok\":false,\"error\":\"no such tool\"}";
    }
    oce_json* a = oce_json_parse(args_json, std::strlen(args_json)); // may be null for malformed input
    std::string r = t->apply(s, a, rng);
    oce_json_free(a);
    return r;
}

static bool ok(const std::string& r) {
    oce_json* j = oce_json_parse(r.c_str(), r.size());
    bool v = oce_json_get_bool(j, "ok", false);
    oce_json_free(j);
    return v;
}

static std::string field_str(const std::string& r, const char* key) {
    oce_json* j = oce_json_parse(r.c_str(), r.size());
    std::string v = oce_json_get_str(j, key, "");
    oce_json_free(j);
    return v;
}

int main(void) {
    using namespace oce;
    Rng rng(123u);

    CHECK(gm_tools().size() == 18u);

    // set_world establishes the setting, a starting location, and an opening hook.
    {
        GameState s;
        CHECK(ok(call("set_world",
                      "{\"world_description\":\"A drowned city of canals.\","
                      "\"starting_location\":\"The Tidewall\","
                      "\"storyline_hook\":\"Bells toll from below the waterline.\"}",
                      s, rng)));
        CHECK(s.world_description == "A drowned city of canals.");
        CHECK(s.world_state.current_location == "The Tidewall");
        CHECK(!s.story.empty()); // the hook is recorded as narration
        CHECK(!ok(call("set_world", "{\"starting_location\":\"x\"}", s, rng))); // description required
    }

    // apply_stat_changes
    {
        GameState s;
        CHECK(ok(call("apply_stat_changes", "{\"hp\":-5,\"gold\":10}", s, rng)));
        CHECK(s.player.hp == 45 && s.player.gold == 60 && s.player.level == 1);
        // 100 xp crosses the level-1 threshold; the level-up also heals +10 hp.
        CHECK(ok(call("apply_stat_changes", "{\"xp\":100}", s, rng)));
        CHECK(s.player.level == 2 && s.player.hp == 55 && s.player.max_hp == 60);
        CHECK(!ok(call("apply_stat_changes", "{bad json", s, rng)));
    }

    // set_suggested_actions
    {
        GameState s;
        CHECK(ok(call("set_suggested_actions", "{\"actions\":[\"Look\",\"Flee\"]}", s, rng)));
        CHECK(s.suggested_actions.size() == 2u);
        CHECK(!ok(call("set_suggested_actions", "{\"actions\":\"nope\"}", s, rng)));
    }

    // start_combat + end_combat
    {
        GameState s;
        CHECK(ok(call("start_combat",
                      "{\"enemies\":[{\"name\":\"Goblin\",\"level\":3},{\"name\":\"Orc\",\"level\":5}]}",
                      s, rng)));
        CHECK(s.combat.active && s.combat.enemies.size() == 2u);
        CHECK(s.combat.enemies[0].hp > 0 && s.combat.enemies[0].max_hp > 0);
        CHECK(!ok(call("start_combat", "{\"enemies\":[]}", s, rng)));     // empty
        CHECK(!ok(call("start_combat", "{}", s, rng)));                    // missing

        const std::string r = call(
            "end_combat",
            "{\"outcome\":\"victory\",\"xp\":100,\"gold\":20,\"loot\":[{\"name\":\"Blade\",\"type\":\"weapon\",\"rarity\":\"rare\",\"power\":2}]}",
            s, rng);
        CHECK(ok(r));
        CHECK(!s.combat.active && s.combat.enemies.empty());
        CHECK(s.player.gold == 70 && s.player.level == 2); // 50 + 20 gold; 100 xp -> level 2
        CHECK(s.inventory.size() == 1u && s.inventory[0].effects.strength == 5); // power 2 + rare 3
        CHECK(!ok(call("end_combat", "{\"outcome\":\"explode\"}", s, rng)));
    }

    // set_skill_check
    {
        GameState s;
        CHECK(ok(call("set_skill_check",
                      "{\"attribute\":\"dexterity\",\"difficulty\":12,\"num_dice\":2}", s, rng)));
        CHECK(s.skill_check.active && s.skill_check.attribute == "dexterity" &&
              s.skill_check.difficulty == 12);
        CHECK(!ok(call("set_skill_check", "{\"attribute\":\"foo\",\"difficulty\":10}", s, rng)));
    }

    // inventory: add / equip / unequip / remove
    {
        GameState s;
        std::string r = call("add_item",
                             "{\"name\":\"Axe\",\"type\":\"weapon\",\"rarity\":\"common\",\"power\":3}",
                             s, rng);
        CHECK(ok(r));
        CHECK(s.inventory.size() == 1u && s.inventory[0].effects.strength == 4); // 3 + common 1
        const std::string id = field_str(r, "item_id");
        CHECK(!id.empty());
        CHECK(!ok(call("add_item", "{\"name\":\"X\",\"type\":\"spork\"}", s, rng))); // bad type

        const std::string equip_args = "{\"item_id\":\"" + id + "\"}";
        CHECK(ok(call("equip_item", equip_args.c_str(), s, rng)));
        CHECK(s.equipment.hand.has_value());
        CHECK(ok(call("unequip_item", "{\"slot\":\"hand\"}", s, rng)));
        CHECK(!s.equipment.hand.has_value());
        CHECK(!ok(call("unequip_item", "{\"slot\":\"hand\"}", s, rng))); // already empty
        CHECK(!ok(call("equip_item", "{\"item_id\":\"missing\"}", s, rng)));

        const std::string remove_args = "{\"item_id\":\"" + id + "\"}";
        CHECK(ok(call("remove_item", remove_args.c_str(), s, rng)));
        CHECK(s.inventory.empty());
        CHECK(!ok(call("remove_item", "{\"item_id\":\"missing\"}", s, rng)));
    }

    // assets & factions
    {
        GameState s;
        CHECK(ok(call("add_business",
                      "{\"name\":\"The Tankard\",\"type\":\"tavern\",\"income_per_day\":10}", s,
                      rng)));
        CHECK(s.assets.businesses.size() == 1u &&
              s.assets.businesses[0].type == BusinessType::Tavern &&
              s.assets.businesses[0].income_per_day == 10);

        CHECK(ok(call("add_relation", "{\"npc_name\":\"Bob\",\"type\":\"ally\",\"strength\":150}", s,
                      rng)));
        CHECK(s.assets.relations.size() == 1u && s.assets.relations[0].strength == 100); // clamped

        CHECK(ok(call("add_property", "{\"name\":\"Cottage\",\"type\":\"house\"}", s, rng)));
        CHECK(s.assets.properties.size() == 1u);

        CHECK(ok(call("add_mount", "{\"name\":\"Steed\",\"type\":\"horse\",\"speed\":1.5}", s, rng)));
        CHECK(s.assets.mounts.size() == 1u && s.assets.mounts[0].condition == 100);

        CHECK(ok(call("change_faction",
                      "{\"faction_id\":\"guild\",\"name\":\"Merchants\",\"relationship_change\":150,\"reputation_change\":2000}",
                      s, rng)));
        CHECK(s.world_state.factions["guild"].relationship == 100); // clamped
        CHECK(s.world_state.factions["guild"].reputation == 1000);  // clamped
        CHECK(s.world_state.factions["guild"].discovered);
        CHECK(!ok(call("change_faction", "{}", s, rng))); // missing faction_id
    }

    // world
    {
        GameState s;
        CHECK(ok(call("upsert_npc", "{\"id\":\"n1\",\"name\":\"Bob\",\"relationship\":50}", s, rng)));
        CHECK(s.world_state.known_npcs.size() == 1u && s.world_state.known_npcs["n1"].name == "Bob");
        CHECK(ok(call("upsert_npc", "{\"id\":\"n1\",\"name\":\"Bobby\"}", s, rng)));
        CHECK(s.world_state.known_npcs["n1"].name == "Bobby"); // updated in place
        CHECK(!ok(call("upsert_npc", "{\"name\":\"NoId\"}", s, rng)));

        CHECK(ok(call("set_location", "{\"location\":\"Riverton\",\"time_elapsed_minutes\":60}", s,
                      rng)));
        CHECK(s.world_state.current_location == "Riverton" && s.world_state.time_elapsed == 60);
        CHECK(!ok(call("set_location", "{}", s, rng)));

        CHECK(ok(call("add_world_fact", "{\"fact\":\"The king is dead.\"}", s, rng)));
        CHECK(s.world_state.world_facts.size() == 1u);
        CHECK(!ok(call("add_world_fact", "{}", s, rng)));
    }

    if (failures == 0) {
        printf("gm: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "gm: %d checks failed\n", failures);
    return 1;
}
