#pragma once
// Snapshot — an immutable, copied view of game state for the UI to render.

#include "oce/model.hpp"

#include <string>
#include <vector>

namespace oce {

struct Snapshot {
    Player player;
    std::vector<Message> story;
    std::vector<std::string> suggested_actions;
    std::string streaming_text; // narrative arriving during the current turn
    bool turn_in_progress = false;
    std::string status;         // a status or error line for the UI
    long long total_tokens = 0;
};

} // namespace oce
