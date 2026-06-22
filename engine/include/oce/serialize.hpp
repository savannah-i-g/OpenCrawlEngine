#pragma once
// Versioned JSON serialization for the full game state.

#include "oce/model.hpp"

#include <string>

namespace oce {

std::string serialize_game_state(const GameState& s);
void        deserialize_game_state(const char* json, GameState& out);

} // namespace oce
