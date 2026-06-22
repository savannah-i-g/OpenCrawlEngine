#include "oce_ui/asset_paths.hpp"

#include <unistd.h>

#include <cstdlib>

// Source-tree assets path, baked in for dev runs; resolved at runtime below.
#ifndef OCE_ASSET_DIR
#define OCE_ASSET_DIR "."
#endif

namespace oce::ui {
namespace fs = std::filesystem;

fs::path executable_dir() {
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) {
        return {};
    }
    buf[n] = '\0';
    return fs::path(buf).parent_path();
}

fs::path asset_dir() {
    std::error_code ec;
    if (const char* env = std::getenv("OCE_ASSET_DIR");
        env != nullptr && env[0] != '\0' && fs::is_directory(fs::path(env), ec)) {
        return fs::path(env);
    }
    const fs::path exe = executable_dir();
    if (!exe.empty()) {
        // Installed/AppImage: <prefix>/bin/<exe> alongside <prefix>/share/...
        const fs::path installed = exe.parent_path() / "share" / "opencrawlengine";
        if (fs::is_directory(installed, ec)) {
            return installed;
        }
        // Portable: assets sitting next to the binary.
        const fs::path portable = exe / "assets";
        if (fs::is_directory(portable, ec)) {
            return portable;
        }
    }
    return fs::path(OCE_ASSET_DIR);
}

fs::path user_data_dir() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        return fs::current_path();
    }
    const fs::path dir = fs::path(home) / ".local" / "share" / "opencrawlengine";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

} // namespace oce::ui
