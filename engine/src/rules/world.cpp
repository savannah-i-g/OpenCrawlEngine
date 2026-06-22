#include "oce/rules/world.hpp"

#include <algorithm>

namespace oce {

int collect_business_income(Business& business, long long now_minutes) {
    if (business.income_per_day <= 0 || now_minutes <= business.last_collected) {
        return 0;
    }
    const long long days = (now_minutes - business.last_collected) / kMinutesPerDay;
    if (days <= 0) {
        return 0;
    }
    business.last_collected += days * kMinutesPerDay; // a partial day carries over
    long long gold = days * (long long) business.income_per_day;
    if (gold > 1000000000LL) {
        gold = 1000000000LL; // defensive cap
    }
    return (int) gold;
}

void change_faction(Faction& faction, int relationship_delta, int reputation_delta) {
    faction.relationship = std::clamp(faction.relationship + relationship_delta, -100, 100);
    faction.reputation = std::clamp(faction.reputation + reputation_delta, 0, 1000);
}

void change_relation_strength(Relation& relation, int delta) {
    relation.strength = std::clamp(relation.strength + delta, -100, 100);
}

void change_mount_condition(MountVehicle& mount, int delta) {
    mount.condition = std::clamp(mount.condition + delta, 0, 100);
}

void set_location(WorldState& world, const std::string& location) {
    world.current_location = location;
    if (std::find(world.visited_locations.begin(), world.visited_locations.end(), location) ==
        world.visited_locations.end()) {
        world.visited_locations.push_back(location);
    }
}

void advance_time(WorldState& world, long long minutes) {
    if (minutes > 0) {
        world.time_elapsed += minutes;
    }
}

void add_world_fact(WorldState& world, const std::string& fact) {
    if (std::find(world.world_facts.begin(), world.world_facts.end(), fact) ==
        world.world_facts.end()) {
        world.world_facts.push_back(fact);
    }
}

void upsert_npc(WorldState& world, const NPC& npc) {
    world.known_npcs[npc.id] = npc;
}

void upsert_faction(WorldState& world, const Faction& faction) {
    world.factions[faction.id] = faction;
}

} // namespace oce
