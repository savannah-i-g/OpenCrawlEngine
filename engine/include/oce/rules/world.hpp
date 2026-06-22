#pragma once
// Asset accrual, relationship clamping, and world-state bookkeeping.

#include "oce/model.hpp"

#include <string>

namespace oce {

constexpr long long kMinutesPerDay = 1440;

// Collects whole-day income earned since the last collection, advancing
// last_collected by the days collected (partial days carry over). Returns the
// gold earned.
int collect_business_income(Business& business, long long now_minutes);

// Whole-day income accrued but not yet collected, without mutating the business.
int pending_business_income(const Business& business, long long now_minutes);

// Relationship/standing changes, clamped to their valid ranges.
void change_faction(Faction& faction, int relationship_delta, int reputation_delta); // -100..100, 0..1000
void change_relation_strength(Relation& relation, int delta);                         // -100..100
void change_mount_condition(MountVehicle& mount, int delta);                          // 0..100

// World-state transitions.
void set_location(WorldState& world, const std::string& location);
void advance_time(WorldState& world, long long minutes);
void add_world_fact(WorldState& world, const std::string& fact);
void upsert_npc(WorldState& world, const NPC& npc);
void upsert_faction(WorldState& world, const Faction& faction);

} // namespace oce
