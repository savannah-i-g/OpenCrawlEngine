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
#include "oce/rules/combat.hpp"
#include "oce/rules/dice.hpp"
#include "oce/rules/worldgen.hpp"
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

struct CampaignParams {
    std::string name = "Adventure";
    std::string theme;
    std::string tone;
    std::vector<std::string> goals;
    Difficulty difficulty = Difficulty::Normal;
    std::string custom_prompt;
};

// A player's combat choice. UseItem consumes a potion as the turn's action.
enum class CombatInput { Attack, Defend, Flee, UseItem };

class Engine {
public:
    explicit Engine(const EngineConfig& cfg);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool     set_api_key(const std::string& key);
    void     set_model(const std::string& model);       // persisted; rebuilds the agent
    void     set_base_url(const std::string& base_url); // persisted; rebuilds the agent
    void     set_theme(const std::string& theme);        // persisted UI theme preference
    // Starts a fresh game: creates the character and starting kit, resets state,
    // and begins a new game-master conversation. Synchronous and local.
    void     new_game(const NewGameParams& params); // new character + first campaign
    std::vector<SaveInfo> list_saves();                  // every campaign in the store
    void     load_save(const std::string& campaign_id);  // load a campaign and its character
    // Character ↔ campaign management.
    std::vector<SaveInfo> list_characters();
    std::vector<SaveInfo> list_campaigns(const std::string& character_id);
    void     new_campaign(const std::string& character_id, const CampaignParams& params);
    void     delete_character(const std::string& character_id);
    void     delete_campaign(const std::string& campaign_id);
    // Generates the opening world from the chosen parameters on the worker
    // thread (the model authors the setting, factions, and starting gear via
    // tools). Non-blocking; progress shows through the snapshot.
    void     generate_world(const WorldParams& params);
    // Asks the model to suggest a value for one world parameter given the
    // others; the result surfaces through the snapshot (autofill_value/seq).
    void     request_autofill(const WorldParams& current, const std::string& field);
    // Acquires a technology/magic-appropriate mount with model-authored flavor,
    // on the worker thread. Non-blocking.
    void     acquire_mount();
    void     submit_turn(const std::string& player_action); // non-blocking
    void     cancel_turn();
    // Enqueues one combat action ("attack"/"defend"/"flee"); the worker resolves
    // the player action and a model-directed enemy phase. No-op if a turn is in
    // progress or combat is not active.
    void     combat_action(const std::string& action, int target_index);
    // Uses a potion as the combat turn's action; the enemy phase follows.
    void     combat_use_item(const std::string& item_id);
    // Rolls the active skill check, records the outcome to the story, and clears it.
    void     resolve_skill_check();
    // Player-driven inventory and progression actions (synchronous, local).
    void     player_equip(const std::string& item_id);
    void     player_unequip(const std::string& slot); // "hand" | "body"
    void     player_consume(const std::string& item_id);
    void     allocate_attribute(const std::string& attribute);
    int      collect_income(); // collects accrued business income; returns gold gained
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
    void        run_worldgen(const WorldParams& params);
    void        run_autofill(const WorldParams& params, const std::string& field);
    void        run_acquire_mount();
    void        run_combat(CombatInput action, int target_index, const std::string& item_id);
    // Asks the model for one action per living enemy (Attack fallback on failure).
    std::vector<EnemyAction> choose_enemy_actions();
    // Seeds the combat outcome into the live game-master conversation.
    void        seed_combat_outcome(CombatOutcomeType outcome);
    bool        ensure_agent();
    void        register_tools();
    // Runs a one-shot model call that is forced to invoke a single named tool,
    // and returns that tool's raw arguments JSON ("" on failure). Worker-thread
    // only; uses a transient agent so the game-master conversation is untouched.
    std::string structured_call(const std::string& system_prompt, const std::string& user_msg,
                                const std::string& tool_name, const std::string& tool_spec_json);
    void        load_saved_state();
    void        persist_settings(const std::string& model, const std::string& base_url,
                                 const std::string& theme);
    std::string system_prompt() const;

    oce_secrets* secrets_ = nullptr;
    oce_http*    http_ = nullptr;
    oce_llm*     llm_ = nullptr;   // worker-thread-owned
    oce_agent*   agent_ = nullptr; // worker-thread-owned
    oce_store*   store_ = nullptr;

    std::string base_url_;
    std::string model_;
    std::string theme_;
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
    std::string  autofill_value_;
    long long    autofill_seq_ = 0;

    std::mutex              turn_mutex_;
    std::condition_variable turn_cv_;
    std::condition_variable idle_cv_;
    std::string             pending_input_;
    bool                    has_pending_ = false;
    WorldParams             pending_world_;
    bool                    has_worldgen_ = false;
    WorldParams             pending_autofill_;
    std::string             pending_autofill_field_;
    bool                    has_autofill_ = false;
    CombatInput             pending_combat_ = CombatInput::Attack;
    int                     pending_combat_target_ = 0;
    std::string             pending_combat_item_;
    bool                    has_combat_ = false;
    bool                    has_mount_ = false;
    bool                    stop_ = false;
    oce_agent_cancel        cancel_{};

    Rng                      rng_;
    std::vector<ToolBinding> tool_bindings_;

    std::thread worker_;
};

} // namespace oce
