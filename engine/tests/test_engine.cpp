// Integration test: drive the engine's agent with a replay backend (no network)
// and confirm tool calls mutate game state, suggested actions update, and the
// state round-trips through the SQLite store across a reopen.
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

typedef struct {
    int turn;
} mock_ctx;

// Turn 1: narrate, change hp, set suggested actions. Turn 2: narrate, stop.
extern "C" int mock_chat(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                         const oce_llm_handlers* h, char* fr, size_t cap) {
    (void) msgs;
    (void) n;
    (void) tools_json;
    mock_ctx* m = (mock_ctx*) ctx;
    ++m->turn;
    if (m->turn == 1) {
        const char* t = "You swing your blade at the goblin.";
        h->on_text(t, strlen(t), h->user);
        h->on_tool_call("c1", "apply_stat_changes", "{\"hp\":-5}", h->user);
        h->on_tool_call("c2", "set_suggested_actions", "{\"actions\":[\"Flee\",\"Fight on\"]}",
                        h->user);
        snprintf(fr, cap, "tool_calls");
    } else {
        const char* t = " The goblin reels from the blow.";
        h->on_text(t, strlen(t), h->user);
        snprintf(fr, cap, "stop");
    }
    return OCE_AGENT_BACKEND_OK;
}

static void cleanup(const char* path) {
    char buf[256];
    remove(path);
    snprintf(buf, sizeof buf, "%s-wal", path);
    remove(buf);
    snprintf(buf, sizeof buf, "%s-shm", path);
    remove(buf);
}

int main(void) {
    const char* db = "/tmp/oce_engine_test.db";
    cleanup(db);

    mock_ctx mc = {0};
    oce_agent_backend backend = {mock_chat, &mc};

    {
        oce::EngineConfig cfg;
        cfg.store_backend = OCE_STORE_SQLITE;
        cfg.db_path = db;
        cfg.test_backend = &backend;
        oce::Engine engine(cfg);

        engine.submit_turn("attack the goblin");
        engine.wait_idle();

        oce::Snapshot s = engine.snapshot();
        CHECK(!s.turn_in_progress);
        CHECK(s.player.hp == 45); // 50 - 5, applied by the tool and clamped
        CHECK(s.suggested_actions.size() == 2u);
        if (s.suggested_actions.size() == 2u) {
            CHECK(s.suggested_actions[0] == "Flee");
            CHECK(s.suggested_actions[1] == "Fight on");
        }
        CHECK(s.story.size() == 2u); // the player line and one narrator line
        if (s.story.size() == 2u) {
            CHECK(s.story[0].sender == "player");
            CHECK(s.story[1].sender == "narrator");
            CHECK(s.story[1].content.find("You swing") != std::string::npos);
            CHECK(s.story[1].content.find("goblin reels") != std::string::npos);
        }
    }

    // Reopen with the same database: state round-trips from the store.
    {
        oce::EngineConfig cfg;
        cfg.store_backend = OCE_STORE_SQLITE;
        cfg.db_path = db;
        oce::Engine engine(cfg);
        oce::Snapshot s = engine.snapshot();
        CHECK(s.player.hp == 45);
        CHECK(s.story.size() == 2u);
    }

    // new_game creates a character with a starting kit and resets state.
    {
        oce::EngineConfig cfg;
        cfg.store_backend = OCE_STORE_MEMORY;
        oce::Engine engine(cfg);
        oce::NewGameParams p;
        p.name = "Lyra";
        p.cls = oce::CharacterClass::Mage;
        p.background = "a wandering scholar";
        p.world_prompt = "a rain-soaked port city";
        engine.new_game(p);
        oce::GameState gs = engine.state_copy();
        CHECK(gs.player.name == "Lyra");
        CHECK(gs.player.cls == oce::CharacterClass::Mage);
        CHECK(gs.player.attributes.intelligence == 8); // the mage starting spread
        CHECK(gs.inventory.size() == 5u);              // starting kit
        CHECK(gs.world_description == "a rain-soaked port city");
        CHECK(!gs.story.empty());
    }

    cleanup(db);

    if (failures == 0) {
        printf("engine: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "engine: %d checks failed\n", failures);
    return 1;
}
