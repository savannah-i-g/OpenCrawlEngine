#pragma once
// The game model. Plain value types; serialization lives in the engine.

#include <string>
#include <vector>

namespace oce {

struct Message {
    std::string sender; // "narrator" | "player" | "system"
    std::string content;
    long long ts = 0;
};

struct Player {
    std::string name = "Adventurer";
    int hp = 50;
    int max_hp = 50;
    int gold = 50;
};

struct GameState {
    Player player;
    std::vector<Message> story;
    std::vector<std::string> suggested_actions;
};

} // namespace oce
