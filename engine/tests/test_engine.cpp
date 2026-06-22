// Integration test: drive the engine's agent with a replay backend (no network)
// and confirm tool calls mutate game state, suggested actions update, and the
// state round-trips through the SQLite store across a reopen.
#include "oce/engine.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

    // Character ↔ campaign management: each new_game makes a character with its
    // first campaign; both characters coexist; loading a campaign restores its
    // character; a second campaign reuses the persistent character; deleting a
    // character drops its campaigns; a restart resumes what was active last.
    {
        const char* db2 = "/tmp/oce_engine_saves.db";
        cleanup(db2);
        std::string alpha_char;
        std::string beta_char;
        std::string alpha_campaign;
        {
            oce::EngineConfig cfg;
            cfg.store_backend = OCE_STORE_SQLITE;
            cfg.db_path = db2;
            oce::Engine engine(cfg);
            oce::NewGameParams a;
            a.name = "Alpha";
            a.cls = oce::CharacterClass::Warrior;
            engine.new_game(a);
            oce::NewGameParams b;
            b.name = "Beta";
            b.cls = oce::CharacterClass::Rogue;
            engine.new_game(b);

            std::vector<oce::SaveInfo> chars = engine.list_characters();
            CHECK(chars.size() == 2u);
            for (const oce::SaveInfo& si : chars) {
                if (si.label.find("Alpha") != std::string::npos) {
                    alpha_char = si.id;
                }
                if (si.label.find("Beta") != std::string::npos) {
                    beta_char = si.id;
                }
            }
            CHECK(!alpha_char.empty() && !beta_char.empty());
            CHECK(engine.state_copy().player.name == "Beta"); // last new_game is active

            std::vector<oce::SaveInfo> alpha_camps = engine.list_campaigns(alpha_char);
            CHECK(alpha_camps.size() == 1u);
            alpha_campaign = alpha_camps[0].id;

            // Loading Alpha's campaign restores Alpha as the active character.
            engine.load_save(alpha_campaign);
            CHECK(engine.state_copy().player.name == "Alpha");

            // A second campaign on Alpha reuses the persistent character.
            oce::CampaignParams cp;
            cp.name = "Second Chapter";
            cp.difficulty = oce::Difficulty::Hard;
            engine.new_campaign(alpha_char, cp);
            CHECK(engine.state_copy().player.name == "Alpha");
            CHECK(engine.state_copy().meta.name == "Second Chapter");
            CHECK(engine.list_campaigns(alpha_char).size() == 2u);

            // Deleting Beta removes the character and its campaign.
            engine.delete_character(beta_char);
            CHECK(engine.list_characters().size() == 1u);

            engine.load_save(alpha_campaign); // make Alpha's first campaign active
            engine.set_model("test/model-x");
            engine.set_theme("Dusk");
        }
        {
            oce::EngineConfig cfg;
            cfg.store_backend = OCE_STORE_SQLITE;
            cfg.db_path = db2;
            oce::Engine engine(cfg);
            CHECK(engine.state_copy().player.name == "Alpha"); // resumes active char+campaign
            CHECK(engine.snapshot().model == "test/model-x");  // provider settings persist
            CHECK(engine.snapshot().theme == "Dusk");          // theme preference persists
        }
        cleanup(db2);
    }

    // Player-driven inventory actions wire through to the rules and persist.
    {
        oce::EngineConfig cfg;
        cfg.store_backend = OCE_STORE_MEMORY;
        oce::Engine engine(cfg);
        oce::NewGameParams p;
        p.name = "Geared";
        engine.new_game(p);
        const size_t kit = engine.state_copy().inventory.size();
        const int str0 = engine.state_copy().player.attributes.strength;

        engine.player_equip("starter-sword");
        {
            oce::GameState gs = engine.state_copy();
            CHECK(gs.equipment.hand.has_value());
            CHECK(gs.equipment.hand.has_value() && gs.equipment.hand->id == "starter-sword");
            CHECK(gs.inventory.size() == kit - 1u);
        }
        engine.player_unequip("hand");
        {
            oce::GameState gs = engine.state_copy();
            CHECK(!gs.equipment.hand.has_value());
            CHECK(gs.inventory.size() == kit); // the sword returned to the pack
        }
        engine.player_consume("starter-health-potion-1");
        CHECK(engine.state_copy().inventory.size() == kit - 1u);
        // No attribute points at level 1: allocation is a no-op.
        engine.allocate_attribute("strength");
        CHECK(engine.state_copy().player.attributes.strength == str0);
    }

    cleanup(db);

    if (failures == 0) {
        printf("engine: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "engine: %d checks failed\n", failures);
    return 1;
}
