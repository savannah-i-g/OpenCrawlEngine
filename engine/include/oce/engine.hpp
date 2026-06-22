#pragma once
// Engine — the single owner of game state and the language-model agent.
//
// Ownership : owns the secret store, HTTP/LLM/agent handles, the SQLite store,
//             the live GameState, and one worker thread.
// Threading : the UI thread calls the public methods; a single worker thread
//             owns the agent and runs each turn. State is published under a
//             mutex; the UI never makes a network call. The llm/agent handles
//             are created and destroyed only on the worker thread.

#include "oce/model.hpp"
#include "oce/rules/dice.hpp"
#include "oce/snapshot.hpp"

#include "oce_agent.h"
#include "oce_store.h"

#include <cstdint>
#include <vector>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

struct oce_secrets;
struct oce_http;
struct oce_llm;

namespace oce {

class Engine;
struct GmTool;

// Binds a registered tool to its owning engine for the dispatch thunk.
struct ToolBinding {
    Engine* engine;
    const GmTool* tool;
};

struct EngineConfig {
    std::string base_url;
    std::string model;
    std::string db_path; // empty -> backend default
    oce_store_backend store_backend = OCE_STORE_SQLITE;
    // When set, the agent uses this backend instead of a live client. For tests.
    const oce_agent_backend* test_backend = nullptr;
    // 0 -> seed the RNG from the system; non-zero for reproducible play and tests.
    uint64_t rng_seed = 0;
};

struct NewGameParams {
    std::string name = "Adventurer";
    CharacterClass cls = CharacterClass::Warrior;
    std::string background;
    std::string world_prompt; // free-text setting the game master builds on
};

struct SaveInfo {
    std::string id;
    std::string label;
};

class Engine {
public:
    explicit Engine(const EngineConfig& cfg);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool     set_api_key(const std::string& key);
    // Starts a fresh game: creates the character and starting kit, resets state,
    // and begins a new game-master conversation. Synchronous and local.
    void     new_game(const NewGameParams& params);
    std::vector<SaveInfo> list_saves();                  // campaigns present in the store
    void     load_save(const std::string& campaign_id);  // switch to and load a campaign
    void     submit_turn(const std::string& player_action); // non-blocking
    void     cancel_turn();
    // Resolves one combat action (attack/defend/flee) synchronously; no-op if a
    // turn is in progress or combat is not active.
    void     combat_action(const std::string& action, int target_index);
    // Rolls the active skill check, records the outcome to the story, and clears it.
    void     resolve_skill_check();
    Snapshot snapshot();
    void     wait_idle(); // blocks until no turn is in progress
    bool     save();

    // Invoked by the C tool/observer thunks on the worker thread.
    std::string dispatch_tool(const GmTool& tool, const char* args_json);
    void        append_stream(const char* data, size_t n);
    GameState   state_copy(); // a locked full copy of game state, for tests/inspection

private:
    void        worker_main();
    void        run_turn(const std::string& input);
    bool        ensure_agent();
    void        register_tools();
    void        load_saved_state();
    std::string system_prompt() const;

    oce_secrets* secrets_ = nullptr;
    oce_http*    http_ = nullptr;
    oce_llm*     llm_ = nullptr;   // worker-thread-owned
    oce_agent*   agent_ = nullptr; // worker-thread-owned
    oce_store*   store_ = nullptr;

    std::string base_url_;
    std::string model_;
    std::string character_id_ = "default";
    std::string campaign_id_ = "default";
    bool               use_test_backend_ = false;
    oce_agent_backend  test_backend_{};

    std::mutex   state_mutex_;
    GameState    state_;
    std::string  streaming_text_;
    std::string  status_;
    bool         turn_in_progress_ = false;
    bool         reload_agent_ = false;
    long long    total_tokens_ = 0;

    std::mutex              turn_mutex_;
    std::condition_variable turn_cv_;
    std::condition_variable idle_cv_;
    std::string             pending_input_;
    bool                    has_pending_ = false;
    bool                    stop_ = false;
    oce_agent_cancel        cancel_{};

    Rng                      rng_;
    std::vector<ToolBinding> tool_bindings_;

    std::thread worker_;
};

} // namespace oce
