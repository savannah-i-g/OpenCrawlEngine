#include "oce/rules/leveling.hpp"

#include <cmath>

namespace oce {

long long xp_for_next_level(int level) {
    if (level < 1) {
        level = 1;
    }
    return (long long) std::floor(100.0 * std::pow((double) level, 1.5));
}

int apply_level_up(Player& player) {
    int gained = 0;
    while (player.xp >= xp_for_next_level(player.level)) {
        player.xp -= xp_for_next_level(player.level);
        player.level += 1;
        player.max_hp += 10;
        player.hp += 10;
        player.max_energy += 5;
        player.energy += 5;
        player.attribute_points += 3;
        ++gained;
    }
    return gained;
}

} // namespace oce
