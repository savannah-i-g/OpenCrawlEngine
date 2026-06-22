#include "oce/engine.hpp"

#include "oce/gm/tools.hpp"
#include "oce/rules/character.hpp"
#include "oce/rules/combat.hpp"
#include "oce/rules/items.hpp"
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
char* oce_engine_capture_thunk(const char* args, void* user);
void oce_engine_thunk_on_text(const char* d, size_t n, void* user);
}

namespace oce {
namespace {

// The engine persists the character and campaign halves separately; the
// full-state (de)serializers in serialize.cpp are used by tests.

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
            const std::string char_id = oce_json_get_str(j, "character", "");
            const std::string camp_id = oce_json_get_str(j, "campaign", "");
            oce_json_free(j);
            if (!char_id.empty()) {
                character_id_ = char_id;
            }
            if (!camp_id.empty()) {
                campaign_id_ = camp_id;
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
        state_.meta = CampaignMeta{};
        character_id_ = "char-" + std::to_string(rng_.between(100000, 999999));
        campaign_id_ = "campaign-" + std::to_string(rng_.between(100000, 999999));
        reload_agent_ = true; // a new game starts a fresh game-master conversation
        streaming_text_.clear();
        status_.clear();
    }
    save();
}

void Engine::generate_world(const WorldParams& params) {
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return;
        }
        turn_in_progress_ = true;
        reload_agent_ = true; // a generated world starts a fresh game-master conversation
        status_ = "Generating world…";
        streaming_text_.clear();
    }
    {
        std::lock_guard<std::mutex> tl(turn_mutex_);
        pending_world_ = params;
        has_worldgen_ = true;
    }
    turn_cv_.notify_one();
}

void Engine::request_autofill(const WorldParams& current, const std::string& field) {
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return; // a turn or world-gen owns the worker; ignore the request
        }
        turn_in_progress_ = true;
        status_ = "Suggesting…";
    }
    {
        std::lock_guard<std::mutex> tl(turn_mutex_);
        pending_autofill_ = current;
        pending_autofill_field_ = field;
        has_autofill_ = true;
    }
    turn_cv_.notify_one();
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
        const int dc = sc.difficulty + difficulty_dc_offset(state_.meta.difficulty);
        const SkillCheckResult r = roll_skill_check(rng_, sc.num_dice, mod, dc);
        const std::string note = "Skill check — " + sc.attribute + " (DC " + std::to_string(dc) +
                                 "): rolled " + std::to_string(r.total) +
                                 (r.success ? " — success." : " — failure.");
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

void Engine::player_equip(const std::string& item_id) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return;
        }
        changed = equip_item(state_.inventory, state_.equipment, item_id);
    }
    if (changed) {
        save();
    }
}

void Engine::player_unequip(const std::string& slot) {
    ItemSlot s;
    if (slot == "hand") {
        s = ItemSlot::Hand;
    } else if (slot == "body") {
        s = ItemSlot::Body;
    } else {
        return;
    }
    bool changed = false;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return;
        }
        changed = unequip_slot(state_.inventory, state_.equipment, s);
    }
    if (changed) {
        save();
    }
}

void Engine::player_consume(const std::string& item_id) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return;
        }
        changed = consume_item(state_.player, state_.inventory, item_id);
    }
    if (changed) {
        save();
    }
}

void Engine::allocate_attribute(const std::string& attribute) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            return;
        }
        changed = oce::allocate_attribute(state_.player, attribute);
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
    s.meta = state_.meta;
    s.autofill_value = autofill_value_;
    s.autofill_seq = autofill_seq_;
    return s;
}

void Engine::wait_idle() {
    std::unique_lock<std::mutex> sl(state_mutex_);
    idle_cv_.wait(sl, [this] { return !turn_in_progress_; });
}

bool Engine::save() {
    std::string char_json;
    std::string campaign_json;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        char_json = serialize_character(state_);
        campaign_json = serialize_campaign(state_);
    }
    if (store_ == nullptr) {
        return false;
    }
    bool ok =
        oce_store_char_upsert(store_, character_id_.c_str(), char_json.c_str(), 1) == OCE_STORE_OK;
    ok = (oce_store_campaign_upsert(store_, campaign_id_.c_str(), character_id_.c_str(),
                                    campaign_json.c_str(), 1) == OCE_STORE_OK) &&
         ok;
    const std::string active = std::string("{\"character\":\"") + character_id_ +
                               "\",\"campaign\":\"" + campaign_id_ + "\"}";
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
            deserialize_campaign(json, gs);
            free(json);
            info.label = gs.meta.name + "  (" + difficulty_to_string(gs.meta.difficulty) + ", " +
                         std::to_string(gs.story.size()) + " entries)";
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
    char* campaign_json = nullptr;
    if (oce_store_campaign_load(store_, id.c_str(), &campaign_json) != OCE_STORE_OK ||
        campaign_json == nullptr) {
        return;
    }
    char* owner = nullptr;
    oce_store_campaign_character(store_, id.c_str(), &owner);
    char* char_json = nullptr;
    if (owner != nullptr && owner[0] != '\0') {
        oce_store_char_load(store_, owner, &char_json);
    }
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            free(campaign_json);
            free(owner);
            free(char_json);
            return;
        }
        state_ = GameState{};
        if (char_json != nullptr) {
            deserialize_character(char_json, state_);
        }
        deserialize_campaign(campaign_json, state_);
        campaign_id_ = id;
        if (owner != nullptr && owner[0] != '\0') {
            character_id_ = owner;
        }
        reload_agent_ = true;
        status_.clear();
        streaming_text_.clear();
    }
    free(campaign_json);
    free(owner);
    free(char_json);
    save(); // record this character + campaign as active
}

std::vector<SaveInfo> Engine::list_characters() {
    std::vector<SaveInfo> out;
    if (store_ == nullptr) {
        return out;
    }
    char** ids = nullptr;
    size_t n = 0;
    if (oce_store_char_list(store_, &ids, &n) != OCE_STORE_OK) {
        return out;
    }
    for (size_t i = 0; i < n; ++i) {
        const std::string id = ids[i];
        if (id == "active" || id == "settings") {
            continue; // engine-internal rows live in the characters table
        }
        SaveInfo info;
        info.id = id;
        char* json = nullptr;
        if (oce_store_char_load(store_, id.c_str(), &json) == OCE_STORE_OK && json != nullptr) {
            GameState gs;
            deserialize_character(json, gs);
            free(json);
            info.label = gs.player.name + "  (Lv " + std::to_string(gs.player.level) + " " +
                         class_to_string(gs.player.cls) + ")";
        } else {
            info.label = id;
        }
        out.push_back(std::move(info));
    }
    oce_store_free_strings(ids, n);
    return out;
}

std::vector<SaveInfo> Engine::list_campaigns(const std::string& character_id) {
    std::vector<SaveInfo> out;
    if (store_ == nullptr) {
        return out;
    }
    char** ids = nullptr;
    size_t n = 0;
    if (oce_store_campaign_list(store_, character_id.c_str(), &ids, &n) != OCE_STORE_OK) {
        return out;
    }
    for (size_t i = 0; i < n; ++i) {
        SaveInfo info;
        info.id = ids[i];
        char* json = nullptr;
        if (oce_store_campaign_load(store_, ids[i], &json) == OCE_STORE_OK && json != nullptr) {
            GameState gs;
            deserialize_campaign(json, gs);
            free(json);
            info.label = gs.meta.name + "  (" + difficulty_to_string(gs.meta.difficulty) + ", " +
                         std::to_string(gs.story.size()) + " entries)";
        } else {
            info.label = ids[i];
        }
        out.push_back(std::move(info));
    }
    oce_store_free_strings(ids, n);
    return out;
}

void Engine::new_campaign(const std::string& character_id, const CampaignParams& params) {
    if (store_ == nullptr) {
        return;
    }
    char* char_json = nullptr;
    if (oce_store_char_load(store_, character_id.c_str(), &char_json) != OCE_STORE_OK ||
        char_json == nullptr) {
        return;
    }
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (turn_in_progress_) {
            free(char_json);
            return;
        }
        GameState fresh;
        deserialize_character(char_json, fresh); // carry the persistent character forward
        state_ = std::move(fresh);
        state_.meta.name = params.name;
        state_.meta.theme = params.theme;
        state_.meta.tone = params.tone;
        state_.meta.goals = params.goals;
        state_.meta.difficulty = params.difficulty;
        state_.meta.custom_prompt = params.custom_prompt;
        state_.story.push_back(Message{"system", "A new chapter begins: " + params.name + ".", 0});
        state_.suggested_actions = {"Look around", "Recall your purpose", "Set out"};
        character_id_ = character_id;
        campaign_id_ = "campaign-" + std::to_string(rng_.between(100000, 999999));
        reload_agent_ = true;
        status_.clear();
        streaming_text_.clear();
    }
    free(char_json);
    save();
}

void Engine::delete_character(const std::string& character_id) {
    if (store_ == nullptr) {
        return;
    }
    char** ids = nullptr;
    size_t n = 0;
    if (oce_store_campaign_list(store_, character_id.c_str(), &ids, &n) == OCE_STORE_OK) {
        for (size_t i = 0; i < n; ++i) {
            oce_store_campaign_delete(store_, ids[i]);
        }
        oce_store_free_strings(ids, n);
    }
    oce_store_char_delete(store_, character_id.c_str());
}

void Engine::delete_campaign(const std::string& campaign_id) {
    if (store_ != nullptr) {
        oce_store_campaign_delete(store_, campaign_id.c_str());
    }
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

    std::string prompt = system_prompt();
    {
        // Weave the active campaign's framing into the game-master brief.
        std::lock_guard<std::mutex> sl(state_mutex_);
        const CampaignMeta& m = state_.meta;
        prompt += "\n\nThis campaign is \"" + m.name + "\"";
        if (!m.theme.empty()) {
            prompt += ", themed around " + m.theme;
        }
        if (!m.tone.empty()) {
            prompt += ", with a " + m.tone + " tone";
        }
        prompt += ". Difficulty: " + std::string(difficulty_to_string(m.difficulty)) + ".";
        if (!m.goals.empty()) {
            prompt += " The player's goals: ";
            for (size_t i = 0; i < m.goals.size(); ++i) {
                prompt += (i != 0 ? "; " : "") + m.goals[i];
            }
            prompt += ".";
        }
        if (!m.custom_prompt.empty()) {
            prompt += " " + m.custom_prompt;
        }
    }
    agent_ = oce_agent_new(backend, prompt.c_str());
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

std::string Engine::structured_call(const std::string& system_prompt, const std::string& user_msg,
                                    const std::string& tool_name,
                                    const std::string& tool_spec_json) {
    oce_agent_backend backend;
    oce_llm* transient = nullptr;
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
            return "";
        }
        const char* extra[] = {"X-Title: OpenCrawlEngine"};
        oce_llm_config cfg;
        cfg.base_url = base_url_copy.c_str();
        cfg.api_key = key;
        cfg.model = model_copy.c_str();
        cfg.extra_headers = extra;
        cfg.extra_header_count = 1;
        transient = oce_llm_new(&cfg, http_);
        oce_secrets_zero(key, sizeof key);
        if (transient == nullptr) {
            return "";
        }
        oce_llm_set_forced_tool(transient, tool_name.c_str()); // force the single tool
        backend = oce_agent_backend_llm(transient);
    }

    oce_agent* ag = oce_agent_new(backend, system_prompt.c_str());
    if (ag == nullptr) {
        if (transient != nullptr) {
            oce_llm_free(transient);
        }
        return "";
    }
    oce_agent_set_max_iterations(ag, 1); // one model call; capture its tool args

    std::string captured;
    oce_agent_tool tool = {tool_name.c_str(), tool_spec_json.c_str(), oce_engine_capture_thunk,
                           &captured};
    oce_agent_add_tool(ag, &tool);
    oce_agent_run(ag, user_msg.c_str(), nullptr, &cancel_);
    oce_agent_free(ag);
    if (transient != nullptr) {
        oce_llm_free(transient);
    }
    return captured;
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

void Engine::run_worldgen(const WorldParams& params) {
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

    std::string brief = "Generate the opening world for a new adventure";
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        brief += " for a level " + std::to_string(state_.player.level) + " " +
                 class_to_string(state_.player.cls) + ".\n";
    }
    auto add_field = [&brief](const char* label, const std::string& value) {
        if (!value.empty()) {
            brief += std::string("- ") + label + ": " + value + "\n";
        }
    };
    add_field("Biome", params.biome);
    add_field("Culture", params.culture);
    add_field("Population", params.population);
    add_field("Technology", params.technology);
    add_field("Politics", params.political);
    add_field("Magic", params.magic);
    add_field("Dominant species", params.species);
    add_field("Primary threat", params.threat);
    for (const std::string& note : params.custom_fields) {
        if (!note.empty()) {
            brief += "- Note: " + note + "\n";
        }
    }
    brief += "\nCall set_world with a vivid world_description, a starting_location, and an opening "
             "storyline_hook. Record two or three durable details with add_world_fact. Introduce a "
             "key faction via change_faction and a notable NPC via upsert_npc. Grant a few fitting "
             "starting items with add_item. Then narrate the opening scene in vivid second person "
             "and finish by calling set_suggested_actions with two to four next actions.";

    oce_agent_observer obs = {oce_engine_thunk_on_text, nullptr, this};
    oce_agent_status st = oce_agent_run(agent_, brief.c_str(), &obs, &cancel_);

    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        if (!streaming_text_.empty()) {
            state_.story.push_back(Message{"narrator", streaming_text_, 0});
        }
        streaming_text_.clear();
        if (st == OCE_AGENT_ERR_CANCELLED) {
            status_ = "World generation cancelled.";
        } else if (st != OCE_AGENT_OK) {
            status_ = "World generation failed. Check your key and model.";
        } else {
            status_.clear();
        }
        if (llm_ != nullptr) {
            total_tokens_ = oce_llm_total_usage(llm_).total_tokens;
        }
        turn_in_progress_ = false;
    }
    save();
    idle_cv_.notify_all();
}

void Engine::run_autofill(const WorldParams& params, const std::string& field) {
    const std::string sys =
        "You are a world-building assistant. Suggest a single concise value for one parameter of a "
        "role-playing setting, consistent with the others. Reply only by calling suggest_value.";
    std::string user = "Suggest a value for the \"" + field + "\" parameter.\nKnown parameters:\n";
    auto add_field = [&user](const char* label, const std::string& value) {
        if (!value.empty()) {
            user += std::string("- ") + label + ": " + value + "\n";
        }
    };
    add_field("Biome", params.biome);
    add_field("Culture", params.culture);
    add_field("Population", params.population);
    add_field("Technology", params.technology);
    add_field("Politics", params.political);
    add_field("Magic", params.magic);
    add_field("Dominant species", params.species);
    add_field("Primary threat", params.threat);
    const char* spec =
        "{\"type\":\"function\",\"function\":{\"name\":\"suggest_value\","
        "\"description\":\"Provide one suggested value for the requested parameter.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},"
        "\"required\":[\"value\"]}}}";
    const std::string args = structured_call(sys, user, "suggest_value", spec);
    std::string value;
    if (!args.empty()) {
        oce_json* j = oce_json_parse(args.c_str(), args.size());
        value = oce_json_get_str(j, "value", "");
        oce_json_free(j);
    }
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        autofill_value_ = value;
        ++autofill_seq_;
        status_.clear();
        turn_in_progress_ = false;
    }
    idle_cv_.notify_all();
}

void Engine::worker_main() {
    enum class Job { Turn, Worldgen, Autofill };
    for (;;) {
        std::string input;
        WorldParams world;
        WorldParams autofill_params;
        std::string autofill_field;
        Job job = Job::Turn;
        {
            std::unique_lock<std::mutex> lk(turn_mutex_);
            turn_cv_.wait(
                lk, [this] { return has_pending_ || has_worldgen_ || has_autofill_ || stop_; });
            if (stop_) {
                return;
            }
            if (has_worldgen_) {
                world = std::move(pending_world_);
                has_worldgen_ = false;
                job = Job::Worldgen;
            } else if (has_autofill_) {
                autofill_params = std::move(pending_autofill_);
                autofill_field = std::move(pending_autofill_field_);
                has_autofill_ = false;
                job = Job::Autofill;
            } else {
                input = std::move(pending_input_);
                has_pending_ = false;
                job = Job::Turn;
            }
        }
        if (job == Job::Worldgen) {
            run_worldgen(world);
        } else if (job == Job::Autofill) {
            run_autofill(autofill_params, autofill_field);
        } else {
            run_turn(input);
        }
    }
}

void Engine::load_saved_state() {
    char* char_json = nullptr;
    if (oce_store_char_load(store_, character_id_.c_str(), &char_json) == OCE_STORE_OK &&
        char_json != nullptr) {
        deserialize_character(char_json, state_);
        free(char_json);
    }
    char* campaign_json = nullptr;
    if (oce_store_campaign_load(store_, campaign_id_.c_str(), &campaign_json) == OCE_STORE_OK &&
        campaign_json != nullptr) {
        deserialize_campaign(campaign_json, state_);
        free(campaign_json);
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

// Captures a forced tool call's arguments for oce::Engine::structured_call.
extern "C" char* oce_engine_capture_thunk(const char* args, void* user) {
    std::string* out = static_cast<std::string*>(user);
    if (args != nullptr) {
        *out = args;
    }
    return dup_cstr("{\"ok\":true}");
}

extern "C" void oce_engine_thunk_on_text(const char* d, size_t n, void* user) {
    static_cast<oce::Engine*>(user)->append_stream(d, n);
}
