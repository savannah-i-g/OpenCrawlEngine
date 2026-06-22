#include "oce/engine.hpp"

#include "oce/rules/character.hpp"
#include "oce/rules/leveling.hpp"

#include "oce_json.h"
#include "oce_llm.h"
#include "oce_secrets.h"

#include <cstdlib>
#include <cstring>
#include <utility>

// C-linkage thunks bridging the agent's callbacks to Engine methods.
extern "C" {
char* oce_engine_thunk_apply_stats(const char* args, void* user);
char* oce_engine_thunk_set_suggested(const char* args, void* user);
void oce_engine_thunk_on_text(const char* d, size_t n, void* user);
}

namespace oce {
namespace {

int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

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

} // namespace

Engine::Engine(const EngineConfig& cfg) {
    base_url_ = !cfg.base_url.empty() ? cfg.base_url : env_or("OCE_BASE_URL", "https://openrouter.ai/api/v1");
    model_ = !cfg.model.empty() ? cfg.model : env_or("OCE_MODEL", "openai/gpt-4o-mini");
    if (cfg.test_backend != nullptr) {
        use_test_backend_ = true;
        test_backend_ = *cfg.test_backend;
    }

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

Snapshot Engine::snapshot() {
    std::lock_guard<std::mutex> sl(state_mutex_);
    Snapshot s;
    s.player = state_.player;
    s.story = state_.story;
    s.suggested_actions = state_.suggested_actions;
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

std::string Engine::tool_apply_stats(const char* args_json) {
    oce_json* a = oce_json_parse(args_json, std::strlen(args_json));
    const int hp = (int) oce_json_get_int(a, "hp", 0);
    const int gold = (int) oce_json_get_int(a, "gold", 0);
    const int energy = (int) oce_json_get_int(a, "energy", 0);
    const long long xp = oce_json_get_int(a, "xp", 0);
    oce_json_free(a);

    int new_hp;
    int new_gold;
    int new_energy;
    int new_level;
    int levels_gained = 0;
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        Player& pl = state_.player;
        pl.hp = clampi(pl.hp + hp, 0, pl.max_hp);
        pl.gold = pl.gold + gold;
        if (pl.gold < 0) {
            pl.gold = 0;
        }
        pl.energy = clampi(pl.energy + energy, 0, pl.max_energy);
        if (xp > 0) {
            pl.xp += xp;
            levels_gained = apply_level_up(pl);
        }
        new_hp = pl.hp;
        new_gold = pl.gold;
        new_energy = pl.energy;
        new_level = pl.level;
    }
    std::string r = "{\"ok\":true,\"hp\":" + std::to_string(new_hp) +
                    ",\"gold\":" + std::to_string(new_gold) +
                    ",\"energy\":" + std::to_string(new_energy) +
                    ",\"level\":" + std::to_string(new_level);
    if (levels_gained > 0) {
        r += ",\"leveled_up\":" + std::to_string(levels_gained);
    }
    r += "}";
    return r;
}

std::string Engine::tool_set_suggested(const char* args_json) {
    oce_json* a = oce_json_parse(args_json, std::strlen(args_json));
    std::vector<std::string> actions;
    const oce_json* arr = oce_json_get(a, "actions");
    if (oce_json_is_array(arr)) {
        size_t n = oce_json_arr_len(arr);
        for (size_t i = 0; i < n && i < 8; ++i) {
            const char* s = oce_json_as_str(oce_json_arr_at(arr, i), "");
            if (s[0] != '\0') {
                actions.emplace_back(s);
            }
        }
    }
    oce_json_free(a);
    {
        std::lock_guard<std::mutex> sl(state_mutex_);
        state_.suggested_actions = std::move(actions);
    }
    return "{\"ok\":true}";
}

void Engine::append_stream(const char* data, size_t n) {
    std::lock_guard<std::mutex> sl(state_mutex_);
    streaming_text_.append(data, n);
}

std::string Engine::system_prompt() const {
    return "You are the game master of a text role-playing game. Narrate the world and the "
           "outcomes of the player's actions in vivid second-person prose, a few sentences at a "
           "time.\n\n"
           "When an action changes the player's health, energy, gold, or experience, call "
           "apply_stat_changes with the signed deltas (positive experience may trigger a "
           "level-up). After narrating, call set_suggested_actions with two to four short actions "
           "the player might take next. Do not state the player's numeric stats in the prose; the "
           "interface displays them.";
}

void Engine::register_tools() {
    static const char* apply_spec =
        "{\"type\":\"function\",\"function\":{\"name\":\"apply_stat_changes\","
        "\"description\":\"Apply signed deltas to the player's hp, energy, gold, and xp.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{"
        "\"hp\":{\"type\":\"integer\",\"description\":\"change to hit points\"},"
        "\"energy\":{\"type\":\"integer\",\"description\":\"change to energy\"},"
        "\"gold\":{\"type\":\"integer\",\"description\":\"change to gold\"},"
        "\"xp\":{\"type\":\"integer\",\"description\":\"experience gained (positive)\"}}}}}";
    static const char* suggest_spec =
        "{\"type\":\"function\",\"function\":{\"name\":\"set_suggested_actions\","
        "\"description\":\"Offer two to four short suggested next actions.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{"
        "\"actions\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},"
        "\"required\":[\"actions\"]}}}";

    oce_agent_tool apply = {"apply_stat_changes", apply_spec, oce_engine_thunk_apply_stats, this};
    oce_agent_tool suggest = {"set_suggested_actions", suggest_spec, oce_engine_thunk_set_suggested,
                              this};
    oce_agent_add_tool(agent_, &apply);
    oce_agent_add_tool(agent_, &suggest);
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

    oce_agent_observer obs = {oce_engine_thunk_on_text, nullptr, this};
    oce_agent_status st = oce_agent_run(agent_, input.c_str(), &obs, &cancel_);

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

extern "C" char* oce_engine_thunk_apply_stats(const char* args, void* user) {
    return dup_cstr(static_cast<oce::Engine*>(user)->tool_apply_stats(args));
}

extern "C" char* oce_engine_thunk_set_suggested(const char* args, void* user) {
    return dup_cstr(static_cast<oce::Engine*>(user)->tool_set_suggested(args));
}

extern "C" void oce_engine_thunk_on_text(const char* d, size_t n, void* user) {
    static_cast<oce::Engine*>(user)->append_stream(d, n);
}
