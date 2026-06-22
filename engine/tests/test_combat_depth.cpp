// Integration test: model-directed enemy tactics. A replay backend starts a
// fight, then directs the lone enemy to defend; the engine applies it
// deterministically (the enemy braces instead of striking).
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

extern "C" int cd_mock(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                       const oce_llm_handlers* h, char* fr, size_t cap) {
    (void) ctx;
    // The enemy-tactics structured call: direct the enemy to defend.
    if (tools_json != nullptr && std::strstr(tools_json, "choose_enemy_actions") != nullptr) {
        h->on_tool_call("e1", "choose_enemy_actions", "{\"actions\":[\"defend\"]}", h->user);
        std::snprintf(fr, cap, "stop");
        return OCE_AGENT_BACKEND_OK;
    }
    // A normal game-master turn: start a fight against one tough foe.
    bool have_tool_result = false;
    for (size_t i = 0; i < n; ++i) {
        if (msgs[i].role != nullptr && std::strcmp(msgs[i].role, "tool") == 0) {
            have_tool_result = true;
            break;
        }
    }
    if (!have_tool_result) {
        h->on_tool_call("c1", "start_combat", "{\"enemies\":[{\"name\":\"Ogre\",\"level\":12}]}",
                        h->user);
        h->on_tool_call("c2", "set_suggested_actions", "{\"actions\":[\"Attack\",\"Flee\"]}",
                        h->user);
        std::snprintf(fr, cap, "tool_calls");
    } else {
        const char* t = "The ogre looms before you.";
        h->on_text(t, std::strlen(t), h->user);
        std::snprintf(fr, cap, "stop");
    }
    return OCE_AGENT_BACKEND_OK;
}

int main(void) {
    oce_agent_backend backend = {cd_mock, nullptr};
    oce::EngineConfig cfg;
    cfg.store_backend = OCE_STORE_MEMORY;
    cfg.test_backend = &backend;
    cfg.rng_seed = 7;
    oce::Engine engine(cfg);

    oce::NewGameParams p;
    p.name = "Brawler";
    p.cls = oce::CharacterClass::Warrior;
    engine.new_game(p);

    engine.submit_turn("provoke a fight");
    engine.wait_idle();
    CHECK(engine.state_copy().combat.active);
    CHECK(engine.state_copy().combat.enemies.size() == 1u);

    const int hp_before = engine.state_copy().player.hp;
    engine.combat_action("attack", 0);
    engine.wait_idle();

    oce::GameState gs = engine.state_copy();
    CHECK(gs.combat.active);                 // a level-12 ogre survives one blow
    CHECK(gs.player.hp == hp_before);        // the directed defend means no enemy strike
    bool defended = false;
    for (const std::string& line : gs.combat.log) {
        if (line.find("braces") != std::string::npos ||
            line.find("recovers") != std::string::npos ||
            line.find("holds back") != std::string::npos) {
            defended = true;
        }
    }
    CHECK(defended);

    if (failures == 0) {
        printf("combat_depth: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "combat_depth: %d checks failed\n", failures);
    return 1;
}
