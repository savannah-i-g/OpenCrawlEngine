#pragma once
// Versioned JSON serialization for the full game state.

#include "oce/model.hpp"

#include <string>

namespace oce {

// The persistent-character half (player, inventory, equipment, assets, world).
std::string serialize_character(const GameState& s);
void        deserialize_character(const char* json, GameState& out);
// The per-adventure campaign half (story, combat, skill check, suggested, meta).
std::string serialize_campaign(const GameState& s);
void        deserialize_campaign(const char* json, GameState& out);
// Both halves in one document (used by the round-trip test).
std::string serialize_game_state(const GameState& s);
void        deserialize_game_state(const char* json, GameState& out);

} // namespace oce
