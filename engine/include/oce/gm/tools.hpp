#pragma once
// The game-master tool registry. Each tool is a model-callable action with an
// OpenAI function spec and a handler that mutates game state and returns a
// compact JSON result. Handlers are pure with respect to threading — the engine
// calls them while holding the state lock.

#include "oce/model.hpp"
#include "oce/rules/dice.hpp"

#include <string>
#include <vector>

struct oce_json; // opaque; full definition in oce_json.h

namespace oce {

struct GmTool {
    const char* name;
    const char* spec_json;
    // Validates `args`, applies the change to `state`, and returns a JSON result
    // ("{\"ok\":true,...}" or "{\"ok\":false,\"error\":...}"). `args` may be null
    // if the model sent malformed JSON.
    std::string (*apply)(GameState& state, const oce_json* args, Rng& rng);
};

// The full set of game-master tools, built once.
const std::vector<GmTool>& gm_tools();

} // namespace oce
