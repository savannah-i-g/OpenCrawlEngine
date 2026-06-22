#include "oce/engine.hpp"

#include "oce/gm/tools.hpp"
#include "oce/rules/character.hpp"
#include "oce/rules/combat.hpp"
#include "oce/rules/skills.hpp"
#include "oce/rules/world.hpp"
#include "oce/serialize.hpp"

#include "oce_json.h"
#include "oce_llm.h"
#include "oce_secrets.h"

#include <cstdlib>
#include <cstring>
#include <random>
#include <utility>

// C-linkage thunks bridging the agent's callbacks to Engine methods.
extern "C" {
char* oce_engine_gm_thunk(const char* args, void* user);
void oce_engine_thunk_on_text(const char* d, size_t n, void* user);
}

namespace oce {
namespace {

std::string serialize_state(const GameState& s) {
    return serialize_game_state(s);
}

void deserialize_state(const char* json, GameState& out) {
    deserialize_game_state(json, out);
}

const char* env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v != nullptr && v[0] != '\0') ? v : fallback;
}

// A compact JSON view of state, given to the model each turn so it can reason
// about the player, location, combat, and inventory (item ids for the tools).
std::string compact_state(const GameState& s) {
    oce_json* root = oce_json_new_object();

    oce_json* p = oce_json_new_object();
    oce_json_obj_set_str(p, "class", class_to_string(s.player.cls));
    oce_json_obj_set_int(p, "level", s.player.level);
    oce_json_obj_set_int(p, "hp", s.player.hp);
    oce_json_obj_set_int(p, "max_hp", s.player.max_hp);
    oce_json_obj_set_int(p, "energy", s.player.energy);
    oce_json_obj_set_int(p, "max_energy", s.player.max_energy);
    oce_json_obj_set_int(p, "gold", s.player.gold);
    oce_json_obj_set(root, "player", p);

    oce_json_obj_set_str(root, "location", s.world_state.current_location.c_str());
    if (!s.world_description.empty()) {
        oce_json_obj_set_str(root, "setting", s.world_description.c_str());
    }
    oce_json_obj_set_bool(root, "in_combat", s.combat.active);
    if (s.combat.active) {
        oce_json* enemies = oce_json_new_array();
        for (const Enemy& e : s.combat.enemies) {
            oce_json* eo = oce_json_new_object();
            oce_json_obj_set_str(eo, "name", e.name.c_str());
            oce_json_obj_set_int(eo, "hp", e.hp);
            oce_json_obj_set_int(eo, "max_hp", e.max_hp);
            oce_json_arr_append(enemies, eo);
        }
        oce_json_obj_set(root, "enemies", enemies);
    }

    oce_json* inv = oce_json_new_array();
    for (const Item& it : s.inventory) {
        oce_json* io = oce_json_new_object();
        oce_json_obj_set_str(io, "id", it.id.c_str());
        oce_json_obj_set_str(io, "name", it.name.c_str());
        oce_json_arr_append(inv, io);
    }
    oce_json_obj_set(root, "inventory", inv);

    char* text = oce_json_print(root, false);
    std::string out = text ? text : "{}";
    free(text);
    oce_json_free(root);
    return out;
}

} // namespace

Engine::Engine(const EngineConfig& cfg) {
    base_url_ = !cfg.base_url.empty() ? cfg.base_url : env_or("OCE_BASE_URL", "https://openrouter.ai/api/v1");
    model_ = !cfg.model.empty() ? cfg.model : env_or("OCE_MODEL", "openai/gpt-4o-mini");
    if (cfg.test_backend != nullptr) {
        use_test_backend_ = true;
        test_backend_ = *cfg.test_backend;
    }
    rng_ = Rng(cfg.rng_seed != 0 ? cfg.rng_seed : (uint64_t) std::random_device{}());

    oce_http_global_init();
    secrets_ = oce_secrets_open();
    http_ = oce_http_new();
    if (http_ != nullptr) {
        oce_http_set_timeouts_ms(http_, 120000, 10000);
        oce_http_set_user_agent(http_, "OpenCrawlEngine");
    }
    store_ = oce_store_open(cfg.db_path.empty() ? nullptr : cfg.db_path.c_str(), cfg.store_backend);

    // Resume the campaign that was active when we last ran, if any.
    {
        char* active = nullptr;
        if (store_ != nullptr && oce_store_char_load(store_, "active", &active) == OCE_STORE_OK &&
            active != nullptr) {
            oce_json* j = oce_json_parse(active, std::strlen(active));
            const std::string cid = oce_json_get_str(j, "campaign", "");
            oce_json_free(j);
            if (!cid.empty()) {
                campaign_id_ = cid;
                character_id_ = cid;
            }
        }
        free(active);
    }
    {
        char* settings = nullptr;
        if (store_ != nullptr &&
            oce_store_char_load(store_, "settings", &settings) == OCE_STORE_OK &&
            settings != nullptr) {
            oce_json* j = oce_json_parse(settings, std::strlen(settings));
            const std::string m = oce_json_get_str(j, "model", "");
            const std::string b = oce_json_get_str(j, "base_url", "");
            oce_json_free(j);
            if (!m.empty()) {
                model_ = m;
            }
            if (!b.empty()) {
                base_url_ = b;
            }
        }
        free(settings);
    }
    load_saved_state();
    oce_secrets_load_env(secrets_, "openrouter", "OPENROUTER_API_KEY");

    worker_ = std::thread(&Engine::worker_main, this);
}

Engine::~Engine() {
    {
        std::lock_guard<std::mutex> tl(turn_mutex_);
        stop_ = true;
    }
    cancel_.flag = 1;
    turn_cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
    // The worker has exited: its handles are now safe to release here.
    if (agent_ != nullptr) {
        oce_agent_free(agent_);
    }
    if (llm_ != nullptr) {
        oce_llm_free(llm_);
    }
    if (store_ != nullptr) {
        oce_store_close(store_);
    }
    if (http_ != nullptr) {
        oce_http_free(http_);
    }
    if (secrets_ != nullptr) {
        oce_secrets_close(secrets_);
    }
    oce_http_global_shutdown();
}

bool Engine::set_api_key(const std::string& key) {
    std::lock_guard<std::mutex> sl(state_mutex_);
    bool ok = oce_secrets_set(secrets_, "openrouter", key.c_str()) == OCE_SECRETS_OK;
    reload_agent_ = true; // rebuild the agent with the new key on the next turn
    return ok;
}

void Engine::persist_settings(const std::string& model, const std::string& base_url) {
    if (store_ == nullptr) {
        return;
    }
    oce_json* o = oce_json_new_object();
    oce_json_obj_set_str(o, "model", model.c_str());
    oce_json_obj_set_str(o, "base_url", base_url.c_str());
    char* text = oce_json_print(o, false);
    if (text != nullptr) {
        oce_store_char_upsert(store_, "settings", text, 1);
        free(text);
    }
    oce_json_free(o);
}

void Engine::set_model(const std::string& model) {
    std::string m;
    std::string b;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        model_ = model;
        reload_agent_ = true;
        m = model_;
        b = base_url_;
    }
    persist_settings(m, b);
}

void Engine::set_base_url(const std::string& base_url) {
    std::string m;
    std::string b;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        base_url_ = base_url;
        reload_agent_ = true;
        m = model_;
        b = base_url_;
    }
    persist_settings(m, b);
}

void Engine::new_game(const NewGameParams& params) {
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return; // do not reset state mid-turn
        }
        state_ = GameState{};
        state_.player = make_character(params.name, params.cls, params.background);
        state_.inventory = starting_kit();
        state_.world_description = params.world_prompt;
        state_.world_state.current_location = "Starting Location";
        std::string opening = "A new adventure begins";
        if (!params.world_prompt.empty()) {
            opening += ": " + params.world_prompt;
        }
        opening += ".";
        state_.story.push_back(Message{"system", opening, 0});
        state_.suggested_actions = {"Look around", "Check your belongings", "Set out"};
        campaign_id_ = "campaign-" + std::to_string(rng_.between(100000, 999999));
        character_id_ = campaign_id_;
        reload_agent_ = true; // a new game starts a fresh game-master conversation
        streaming_text_.clear();
        status_.clear();
    }
    save();
}

void Engine::submit_turn(const std::string& player_action) {
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return;
        }
        state_.story.push_back(Message{"player", player_action, 0});
        streaming_text_.clear();
        status_.clear();
        turn_in_progress_ = true;
    }
    {
        std::lock_guard<std::mutex> tl(turn_mutex_);
        pending_input_ = player_action;
        has_pending_ = true;
    }
    turn_cv_.notify_one();
}

void Engine::cancel_turn() {
    cancel_.flag = 1;
}

void Engine::combat_action(const std::string& action, int target_index) {
    CombatAction a;
    if (action == "attack") {
        a = CombatAction::Attack;
    } else if (action == "defend") {
        a = CombatAction::Defend;
    } else if (action == "flee") {
        a = CombatAction::Flee;
    } else {
        return;
    }
    bool ended = false;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_ || !state_.combat.active) {
            return;
        }
        ended = resolve_combat_turn(state_, rng_, a, target_index).combat_ended;
    }
    if (ended) {
        save();
    }
}

void Engine::resolve_skill_check() {
    bool changed = false;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        SkillCheck& sc = state_.skill_check;
        if (turn_in_progress_ || !sc.active) {
            return;
        }
        const int mod = modifier(attribute_value(state_.player.attributes, sc.attribute));
        const SkillCheckResult r = roll_skill_check(rng_, sc.num_dice, mod, sc.difficulty);
        const std::string note = "Skill check — " + sc.attribute + " (DC " +
                                 std::to_string(sc.difficulty) + "): rolled " +
                                 std::to_string(r.total) + (r.success ? " — success." : " — failure.");
        state_.story.push_back(Message{"system", note, 0});
        const std::string branch = r.success ? sc.on_success : sc.on_failure;
        if (!branch.empty()) {
            state_.story.push_back(Message{"narrator", branch, 0});
        }
        sc = SkillCheck{};
        changed = true;
    }
    if (changed) {
        save();
    }
}

Snapshot Engine::snapshot() {
    std::lock_guard<std::mutex> sl(state_mutex_);
    Snapshot s;
    s.player = state_.player;
    s.inventory = state_.inventory;
    s.equipment = state_.equipment;
    s.assets = state_.assets;
    s.story = state_.story;
    s.suggested_actions = state_.suggested_actions;
    s.combat = state_.combat;
    s.skill_check = state_.skill_check;
    s.world_state = state_.world_state;
    s.streaming_text = streaming_text_;
    s.turn_in_progress = turn_in_progress_;
    s.status = status_;
    s.total_tokens = total_tokens_;
    s.model = model_;
    s.base_url = base_url_;
    return s;
}

void Engine::wait_idle() {
    std::unique_lock<std::mutex> sl(state_mutex_);
    idle_cv_.wait(sl, [this] { return !turn_in_progress_; });
}

bool Engine::save() {
    std::string json;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        json = serialize_state(state_);
    }
    if (store_ == nullptr) {
        return false;
    }
    bool ok = oce_store_char_upsert(store_, character_id_.c_str(), "{\"name\":\"default\"}", 1) ==
              OCE_STORE_OK;
    ok = (oce_store_campaign_upsert(store_, campaign_id_.c_str(), character_id_.c_str(), json.c_str(),
                                    1) == OCE_STORE_OK) &&
         ok;
    const std::string active = std::string("{\"campaign\":\"") + campaign_id_ + "\"}";
    oce_store_char_upsert(store_, "active", active.c_str(), 1);
    return ok;
}

std::vector<SaveInfo> Engine::list_saves() {
    std::vector<SaveInfo> out;
    if (store_ == nullptr) {
        return out;
    }
    char** ids = nullptr;
    size_t n = 0;
    if (oce_store_campaign_list(store_, nullptr, &ids, &n) != OCE_STORE_OK) {
        return out;
    }
    for (size_t i = 0; i < n; ++i) {
        SaveInfo info;
        info.id = ids[i];
        char* json = nullptr;
        if (oce_store_campaign_load(store_, ids[i], &json) == OCE_STORE_OK && json != nullptr) {
            GameState gs;
            deserialize_game_state(json, gs);
            free(json);
            info.label = gs.player.name + "  (Lv " + std::to_string(gs.player.level) + " " +
                         class_to_string(gs.player.cls) + ", " + gs.world_state.current_location +
                         ")";
        } else {
            info.label = ids[i];
        }
        out.push_back(std::move(info));
    }
    oce_store_free_strings(ids, n);
    return out;
}

void Engine::load_save(const std::string& id) {
    if (store_ == nullptr) {
        return;
    }
    char* json = nullptr;
    if (oce_store_campaign_load(store_, id.c_str(), &json) != OCE_STORE_OK || json == nullptr) {
        return;
    }
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            free(json);
            return;
        }
        state_ = GameState{};
        deserialize_game_state(json, state_);
        campaign_id_ = id;
        character_id_ = id;
        reload_agent_ = true;
        status_.clear();
        streaming_text_.clear();
    }
    free(json);
    save(); // record this campaign as active
}

std::string Engine::dispatch_tool(const GmTool& tool, const char* args_json) {
    oce_json* parsed = oce_json_parse(args_json, std::strlen(args_json));
    std::string result;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        result = tool.apply(state_, parsed, rng_);
    }
    oce_json_free(parsed);
    return result;
}

GameState Engine::state_copy() {
    std::lock_guard<std::mutex> sl(state_mutex_);
    return state_;
}

void Engine::append_stream(const char* data, size_t n) {
    std::lock_guard<std::mutex> sl(state_mutex_);
    streaming_text_.append(data, n);
}

std::string Engine::system_prompt() const {
    return "You are the game master of a text role-playing game. Narrate the world and the "
           "outcomes of the player's actions in vivid second-person prose, a few sentences at a "
           "time. Each turn you are given the current game state as JSON; use it to stay "
           "consistent.\n\n"
           "Drive the game through tools:\n"
           "- apply_stat_changes: signed deltas to hp, energy, gold, or xp (positive xp may level "
           "the player up).\n"
           "- start_combat / end_combat: begin an encounter (give each enemy a name and level; the "
           "engine sets their stats) and resolve it (outcome plus any xp, gold, and loot).\n"
           "- set_skill_check: when an action is uncertain, request a check on an attribute against "
           "a difficulty; the engine rolls the dice.\n"
           "- add_item / remove_item / equip_item / unequip_item: manage the inventory by item id.\n"
           "- add_business / add_relation / add_property / add_mount / change_faction: grant "
           "holdings and adjust standing.\n"
           "- upsert_npc / set_location / add_world_fact: keep the world consistent.\n"
           "Finish each turn by calling set_suggested_actions with two to four short next "
           "actions.\n\n"
           "The engine owns all randomness: never invent dice results or decide whether an attack "
           "or skill check succeeds — call the tool and react to its result. Do not state the "
           "player's numeric stats in the prose; the interface shows them.";
}

void Engine::register_tools() {
    const std::vector<GmTool>& tools = gm_tools();
    tool_bindings_.clear();
    tool_bindings_.reserve(tools.size()); // stable addresses for the agent's borrowed user pointers
    for (const GmTool& t : tools) {
        tool_bindings_.push_back(ToolBinding{this, &t});
    }
    for (size_t i = 0; i < tools.size(); ++i) {
        oce_agent_tool at = {tools[i].name, tools[i].spec_json, oce_engine_gm_thunk,
                             &tool_bindings_[i]};
        oce_agent_add_tool(agent_, &at);
    }
}

bool Engine::ensure_agent() {
    if (agent_ != nullptr) {
        return true;
    }

    oce_agent_backend backend;
    if (use_test_backend_) {
        backend = test_backend_;
    } else {
        char key[1024];
        bool have_key;
        std::string base_url_copy;
        std::string model_copy;
        {
            std::lock_guard<std::mutex> sl(state_mutex_);
            have_key = oce_secrets_get(secrets_, "openrouter", key, sizeof key) == OCE_SECRETS_OK;
            base_url_copy = base_url_;
            model_copy = model_;
        }
        if (!have_key) {
            return false;
        }
        const char* extra[] = {"X-Title: OpenCrawlEngine"};
        oce_llm_config cfg;
        cfg.base_url = base_url_copy.c_str();
        cfg.api_key = key;
        cfg.model = model_copy.c_str();
        cfg.extra_headers = extra;
        cfg.extra_header_count = 1;
        llm_ = oce_llm_new(&cfg, http_);
        oce_secrets_zero(key, sizeof key);
        if (llm_ == nullptr) {
            return false;
        }
        backend = oce_agent_backend_llm(llm_);
    }

    agent_ = oce_agent_new(backend, system_prompt().c_str());
    if (agent_ == nullptr) {
        return false;
    }
    register_tools();
    // Prime the fresh agent with recent story so a resumed game keeps context.
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        const size_t count = state_.story.size();
        const size_t start = (count > 12) ? count - 12 : 0;
        for (size_t i = start; i < count; ++i) {
            const Message& m = state_.story[i];
            const char* role = (m.sender == "player")  ? "user"
                               : (m.sender == "system") ? "system"
                                                        : "assistant";
            oce_agent_seed_message(agent_, role, m.content.c_str());
        }
    }
    return true;
}

void Engine::run_turn(const std::string& input) {
    cancel_.flag = 0;

    bool reload;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        reload = reload_agent_;
        reload_agent_ = false;
    }
    if (reload) {
        if (agent_ != nullptr) {
            oce_agent_free(agent_);
            agent_ = nullptr;
        }
        if (llm_ != nullptr) {
            oce_llm_free(llm_);
            llm_ = nullptr;
        }
    }

    if (!ensure_agent()) {
        {
            std::lock_guard<std::mutex> sl(state_mutex_);
            status_ = "Set your OpenRouter API key in Settings to begin.";
            turn_in_progress_ = false;
        }
        idle_cv_.notify_all();
        return;
    }

    const std::string turn_message =
        "Current game state:\n" + compact_state(state_copy()) +
        "\n\nThe player attempts the following. Narrate the outcome and call tools as needed:\n" +
        input;
    oce_agent_observer obs = {oce_engine_thunk_on_text, nullptr, this};
    oce_agent_status st = oce_agent_run(agent_, turn_message.c_str(), &obs, &cancel_);

    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (!streaming_text_.empty()) {
            state_.story.push_back(Message{"narrator", streaming_text_, 0});
        }
        streaming_text_.clear();
        if (st == OCE_AGENT_ERR_CANCELLED) {
            status_ = "Turn cancelled.";
        } else if (st != OCE_AGENT_OK) {
            status_ = "The game master could not respond. Check your key and model.";
        }
        if (llm_ != nullptr) {
            total_tokens_ = oce_llm_total_usage(llm_).total_tokens;
        }
        int income = 0;
        for (Business& b : state_.assets.businesses) {
            income += collect_business_income(b, state_.world_state.time_elapsed);
        }
        if (income > 0) {
            state_.player.gold += income;
            state_.story.push_back(
                Message{"system", "Your holdings bring in " + std::to_string(income) + " gold.", 0});
        }
        turn_in_progress_ = false;
    }
    save();
    idle_cv_.notify_all();
}

void Engine::worker_main() {
    for (;;) {
        std::string input;
        {
            std::unique_lock<std::mutex> lk(turn_mutex_);
            turn_cv_.wait(lk, [this] { return has_pending_ || stop_; });
            if (stop_) {
                return;
            }
            input = std::move(pending_input_);
            has_pending_ = false;
        }
        run_turn(input);
    }
}

void Engine::load_saved_state() {
    char* json = nullptr;
    if (oce_store_campaign_load(store_, campaign_id_.c_str(), &json) == OCE_STORE_OK &&
        json != nullptr) {
        deserialize_state(json, state_);
        free(json);
    }
}

} // namespace oce

// --- C-linkage thunks -------------------------------------------------------

static char* dup_cstr(const std::string& s) {
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (p != nullptr) {
        std::memcpy(p, s.c_str(), s.size() + 1);
    }
    return p;
}

extern "C" char* oce_engine_gm_thunk(const char* args, void* user) {
    oce::ToolBinding* b = static_cast<oce::ToolBinding*>(user);
    return dup_cstr(b->engine->dispatch_tool(*b->tool, args));
}

extern "C" void oce_engine_thunk_on_text(const char* d, size_t n, void* user) {
    static_cast<oce::Engine*>(user)->append_stream(d, n);
}
