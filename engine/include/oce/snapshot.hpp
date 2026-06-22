#pragma once
// Snapshot — an immutable, copied view of game state for the UI to render.

#include "oce/model.hpp"

#include <string>
#include <vector>

namespace oce {

struct Snapshot {
    Player player;
    std::vector<Item> inventory;
    Equipment equipment;
    PlayerAssets assets;
    std::vector<Message> story;
    std::vector<std::string> suggested_actions;
    CombatState combat;
    SkillCheck skill_check;
    WorldState world_state;
    CampaignMeta meta;          // the active campaign's framing
    std::string streaming_text; // narrative arriving during the current turn
    bool turn_in_progress = false;
    std::string status;         // a status or error line for the UI
    long long total_tokens = 0;
    std::string model;
    std::string base_url;
    std::string theme; // persisted UI theme name
    // Latest world-parameter autofill suggestion; seq increments per result so
    // the UI can detect and apply a new suggestion.
    std::string autofill_value;
    long long autofill_seq = 0;
};

} // namespace oce
