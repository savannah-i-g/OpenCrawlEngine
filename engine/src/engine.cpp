#include "oce/engine.hpp"

#include "oce/gm/tools.hpp"
#include "oce/rules/character.hpp"
#include "oce/rules/combat.hpp"
#include "oce/rules/skills.hpp"

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
    oce_json* root = oce_json_new_object();
    oce_json* p = oce_json_new_object();
    oce_json_obj_set_str(p, "name", s.player.name.c_str());
    oce_json_obj_set_str(p, "class", class_to_string(s.player.cls));
    oce_json_obj_set_int(p, "level", s.player.level);
    oce_json_obj_set_int(p, "xp", s.player.xp);
    oce_json_obj_set_int(p, "gold", s.player.gold);
    oce_json_obj_set_int(p, "hp", s.player.hp);
    oce_json_obj_set_int(p, "max_hp", s.player.max_hp);
    oce_json_obj_set_int(p, "energy", s.player.energy);
    oce_json_obj_set_int(p, "max_energy", s.player.max_energy);
    oce_json_obj_set_int(p, "attribute_points", s.player.attribute_points);
    if (!s.player.background.empty()) {
        oce_json_obj_set_str(p, "background", s.player.background.c_str());
    }
    oce_json* attrs = oce_json_new_object();
    oce_json_obj_set_int(attrs, "strength", s.player.attributes.strength);
    oce_json_obj_set_int(attrs, "dexterity", s.player.attributes.dexterity);
    oce_json_obj_set_int(attrs, "intelligence", s.player.attributes.intelligence);
    oce_json_obj_set_int(attrs, "constitution", s.player.attributes.constitution);
    oce_json_obj_set_int(attrs, "wisdom", s.player.attributes.wisdom);
    oce_json_obj_set_int(attrs, "charisma", s.player.attributes.charisma);
    oce_json_obj_set_int(attrs, "luck", s.player.attributes.luck);
    oce_json_obj_set_int(attrs, "perception", s.player.attributes.perception);
    oce_json_obj_set_int(attrs, "stealth", s.player.attributes.stealth);
    oce_json_obj_set_int(attrs, "bartering", s.player.attributes.bartering);
    oce_json_obj_set(p, "attributes", attrs);
    oce_json_obj_set(root, "player", p);

    oce_json* story = oce_json_new_array();
    for (const Message& m : s.story) {
        oce_json* mo = oce_json_new_object();
        oce_json_obj_set_str(mo, "sender", m.sender.c_str());
        oce_json_obj_set_str(mo, "content", m.content.c_str());
        oce_json_arr_append(story, mo);
    }
    oce_json_obj_set(root, "story", story);

    oce_json* sa = oce_json_new_array();
    for (const std::string& a : s.suggested_actions) {
        oce_json_arr_append_str(sa, a.c_str());
    }
    oce_json_obj_set(root, "suggested_actions", sa);

    char* text = oce_json_print(root, false);
    std::string out = text ? text : "{}";
    free(text);
    oce_json_free(root);
    return out;
}

void deserialize_state(const char* json, GameState& out) {
    oce_json* root = oce_json_parse(json, std::strlen(json));
    if (root == nullptr) {
        return;
    }
    const oce_json* p = oce_json_get(root, "player");
    if (oce_json_is_object(p)) {
        out.player.name = oce_json_get_str(p, "name", "Adventurer");
        CharacterClass cls;
        if (class_from_string(oce_json_get_str(p, "class", "warrior"), cls)) {
            out.player.cls = cls;
        }
        out.player.level = (int) oce_json_get_int(p, "level", out.player.level);
        out.player.xp = oce_json_get_int(p, "xp", out.player.xp);
        out.player.gold = (int) oce_json_get_int(p, "gold", out.player.gold);
        out.player.hp = (int) oce_json_get_int(p, "hp", out.player.hp);
        out.player.max_hp = (int) oce_json_get_int(p, "max_hp", out.player.max_hp);
        out.player.energy = (int) oce_json_get_int(p, "energy", out.player.energy);
        out.player.max_energy = (int) oce_json_get_int(p, "max_energy", out.player.max_energy);
        out.player.attribute_points =
            (int) oce_json_get_int(p, "attribute_points", out.player.attribute_points);
        out.player.background = oce_json_get_str(p, "background", "");
        const oce_json* attrs = oce_json_get(p, "attributes");
        if (oce_json_is_object(attrs)) {
            Attributes& at = out.player.attributes;
            at.strength = (int) oce_json_get_int(attrs, "strength", at.strength);
            at.dexterity = (int) oce_json_get_int(attrs, "dexterity", at.dexterity);
            at.intelligence = (int) oce_json_get_int(attrs, "intelligence", at.intelligence);
            at.constitution = (int) oce_json_get_int(attrs, "constitution", at.constitution);
            at.wisdom = (int) oce_json_get_int(attrs, "wisdom", at.wisdom);
            at.charisma = (int) oce_json_get_int(attrs, "charisma", at.charisma);
            at.luck = (int) oce_json_get_int(attrs, "luck", at.luck);
            at.perception = (int) oce_json_get_int(attrs, "perception", at.perception);
            at.stealth = (int) oce_json_get_int(attrs, "stealth", at.stealth);
            at.bartering = (int) oce_json_get_int(attrs, "bartering", at.bartering);
        }
    }
    const oce_json* story = oce_json_get(root, "story");
    if (oce_json_is_array(story)) {
        out.story.clear();
        size_t n = oce_json_arr_len(story);
        for (size_t i = 0; i < n; ++i) {
            const oce_json* m = oce_json_arr_at(story, i);
            Message msg;
            msg.sender = oce_json_get_str(m, "sender", "narrator");
            msg.content = oce_json_get_str(m, "content", "");
            out.story.push_back(std::move(msg));
        }
    }
    const oce_json* sa = oce_json_get(root, "suggested_actions");
    if (oce_json_is_array(sa)) {
        out.suggested_actions.clear();
        size_t n = oce_json_arr_len(sa);
        for (size_t i = 0; i < n; ++i) {
            out.suggested_actions.push_back(oce_json_as_str(oce_json_arr_at(sa, i), ""));
        }
    }
    oce_json_free(root);
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
    s.story = state_.story;
    s.suggested_actions = state_.suggested_actions;
    s.combat = state_.combat;
    s.skill_check = state_.skill_check;
    s.streaming_text = streaming_text_;
    s.turn_in_progress = turn_in_progress_;
    s.status = status_;
    s.total_tokens = total_tokens_;
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
    return ok;
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
        {
            std::lock_guard<std::mutex> sl(state_mutex_);
            have_key = oce_secrets_get(secrets_, "openrouter", key, sizeof key) == OCE_SECRETS_OK;
        }
        if (!have_key) {
            return false;
        }
        const char* extra[] = {"X-Title: OpenCrawlEngine"};
        oce_llm_config cfg;
        cfg.base_url = base_url_.c_str();
        cfg.api_key = key;
        cfg.model = model_.c_str();
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
