#pragma once
// Experience and level progression.

#include "oce/model.hpp"

namespace oce {

// XP required to advance from `level` to the next: floor(100 * level^1.5).
long long xp_for_next_level(int level);

// Spends accumulated XP to apply as many level-ups as possible. Each level
// grants +10 max HP, +5 max energy, and +3 attribute points; current HP and
// energy rise by the same amounts. Returns the number of levels gained.
int apply_level_up(Player& player);

} // namespace oce
