#pragma once
// Version information for the engine library.

namespace oce {

inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

// Returns the semantic version as a "MAJOR.MINOR.PATCH" string.
const char* version_string();

} // namespace oce
