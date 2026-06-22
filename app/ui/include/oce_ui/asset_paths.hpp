#pragma once
// Runtime resolution of resource locations, so the frontend finds its icons and
// fonts whether it runs from the build tree, an installed prefix, or an AppImage.
// All returned paths are absolute. Lifetime: free functions, no state.
// Threading: call from the UI thread before the first frame.

#include <filesystem>

namespace oce::ui {

// Directory containing the running executable, resolved via /proc/self/exe.
// Empty if it cannot be determined.
std::filesystem::path executable_dir();

// Base directory holding bundled assets, with `icons/` and (in an AppImage)
// `fonts/` subdirectories. First existing of: the OCE_ASSET_DIR environment
// override, <exe>/../share/opencrawlengine (installed and AppImage layout),
// <exe>/assets (portable, next to the binary), then the compile-time
// OCE_ASSET_DIR (the source tree, for dev builds). Callers append `icons` etc.
std::filesystem::path asset_dir();

// Per-user data directory (saves, settings, ImGui layout), created if needed.
// Falls back to the current directory when HOME is unset.
std::filesystem::path user_data_dir();

} // namespace oce::ui
