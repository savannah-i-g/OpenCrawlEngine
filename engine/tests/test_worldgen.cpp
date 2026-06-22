// Integration test: drive world generation and parameter autofill with a replay
// backend (no network). World-gen seeds the world, a faction, starting gear, and
// suggested actions via the existing tools; autofill returns a suggestion.
#include "oce/engine.hpp"

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

// Responds by inspecting the request: an autofill call (its only tool is
// suggest_value) returns a value; a world-gen call returns world-building tool
// calls first, then narration once the tool results are in the history.
extern "C" int wg_mock(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                       const oce_llm_handlers* h, char* fr, size_t cap) {
    (void) ctx;
    if (tools_json != nullptr && std::strstr(tools_json, "suggest_value") != nullptr) {
        h->on_tool_call("a1", "suggest_value",
                        "{\"value\":\"The Floating Isles of Aether\"}", h->user);
        std::snprintf(fr, cap, "stop");
        return OCE_AGENT_BACKEND_OK;
    }

    bool have_tool_result = false;
    for (size_t i = 0; i < n; ++i) {
        if (msgs[i].role != nullptr && std::strcmp(msgs[i].role, "tool") == 0) {
            have_tool_result = true;
            break;
        }
    }
    if (!have_tool_result) {
        h->on_tool_call("w1", "set_world",
                        "{\"world_description\":\"A sky realm of floating isles.\","
                        "\"starting_location\":\"Skyport Lumen\","
                        "\"storyline_hook\":\"Airships vanish over the Rift.\"}",
                        h->user);
        h->on_tool_call("w2", "add_item",
                        "{\"name\":\"Aether Compass\",\"type\":\"weapon\",\"rarity\":\"uncommon\","
                        "\"power\":3}",
                        h->user);
        h->on_tool_call("w3", "change_faction",
                        "{\"faction_id\":\"skyguard\",\"name\":\"The Skyguard\","
                        "\"relationship_change\":10}",
                        h->user);
        h->on_tool_call("w4", "set_suggested_actions",
                        "{\"actions\":[\"Visit the Skyport\",\"Ask about the Rift\"]}", h->user);
        std::snprintf(fr, cap, "tool_calls");
    } else {
        const char* t = "You stand at Skyport Lumen as airships drift overhead.";
        h->on_text(t, std::strlen(t), h->user);
        std::snprintf(fr, cap, "stop");
    }
    return OCE_AGENT_BACKEND_OK;
}

int main(void) {
    oce_agent_backend backend = {wg_mock, nullptr};

    oce::EngineConfig cfg;
    cfg.store_backend = OCE_STORE_MEMORY;
    cfg.test_backend = &backend;
    oce::Engine engine(cfg);

    oce::NewGameParams p;
    p.name = "Wren";
    p.cls = oce::CharacterClass::Ranger;
    engine.new_game(p);
    const size_t kit_size = engine.state_copy().inventory.size();

    oce::WorldParams wp;
    wp.biome = "Coastal";
    wp.magic = "Common (Widely practiced)";
    engine.generate_world(wp);
    engine.wait_idle();

    oce::GameState gs = engine.state_copy();
    CHECK(gs.world_description.find("floating isles") != std::string::npos);
    CHECK(gs.world_state.current_location == "Skyport Lumen");
    CHECK(!gs.world_state.factions.empty());
    CHECK(gs.inventory.size() == kit_size + 1u); // starting kit plus the granted item
    bool has_compass = false;
    for (const oce::Item& it : gs.inventory) {
        if (it.name == "Aether Compass") {
            has_compass = true;
        }
    }
    CHECK(has_compass);
    CHECK(gs.suggested_actions.size() == 2u);
    bool narrated = false;
    for (const oce::Message& m : gs.story) {
        if (m.content.find("Skyport Lumen") != std::string::npos) {
            narrated = true;
        }
    }
    CHECK(narrated);

    // Autofill returns a suggestion through the snapshot.
    engine.request_autofill(wp, "premise");
    engine.wait_idle();
    oce::Snapshot s = engine.snapshot();
    CHECK(s.autofill_seq >= 1);
    CHECK(!s.autofill_value.empty());

    if (failures == 0) {
        printf("worldgen: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "worldgen: %d checks failed\n", failures);
    return 1;
}
