#pragma once
// Character creation: per-class starting attributes, the initial player, and
// the starting inventory.

#include "oce/model.hpp"

#include <string>
#include <vector>

namespace oce {

Attributes starting_attributes(CharacterClass cls);
Player      make_character(const std::string& name, CharacterClass cls, const std::string& background);
std::vector<Item> starting_kit();

const char* class_to_string(CharacterClass cls);
bool        class_from_string(const std::string& s, CharacterClass& out);

const char* difficulty_to_string(Difficulty d);
bool        difficulty_from_string(const std::string& s, Difficulty& out);

} // namespace oce
