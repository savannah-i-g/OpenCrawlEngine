#pragma once
// Mount and vehicle rosters keyed by world technology, with fantasy mounts
// gated by magic. Pure data plus a lookup; unique names/descriptions are layered
// on separately by the engine's model-driven flavor pass.

#include "oce/model.hpp"

#include <string>
#include <vector>

namespace oce {

// Candidate mounts for a world of the given technology level (an unknown level
// falls back to the medieval roster). Fantasy mounts are appended when magic is
// "Common (Widely practiced)" or "Ubiquitous (Everyday occurrence)". Returned
// templates carry type/name/description/speed/capacity/upkeep but no id.
std::vector<MountVehicle> available_mounts(const std::string& technology, const std::string& magic);

} // namespace oce
