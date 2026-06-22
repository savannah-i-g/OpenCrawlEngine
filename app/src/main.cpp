// OpenCrawlEngine — application entry point.
//
// Builds the engine and UI, then runs the frame loop until the window closes.
// Pass --frames=N to render N frames and exit (used for automated smoke runs).

#include "oce/engine.hpp"
#include "oce_ui/game_panels.hpp"
#include "oce_ui/ui_app.hpp"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

// The save database lives under the user's data directory. Best-effort dir
// creation; failures fall back to the working directory.
std::string default_db_path() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        return "oce_save.db";
    }
    const std::string base = std::string(home) + "/.local";
    mkdir(base.c_str(), 0755);
    mkdir((base + "/share").c_str(), 0755);
    const std::string dir = base + "/share/opencrawlengine";
    mkdir(dir.c_str(), 0755);
    return dir + "/oce.db";
}

} // namespace

int main(int argc, char** argv) {
    int max_frames = -1; // negative: run until the window is closed
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--frames=", 0) == 0) {
            max_frames = std::atoi(arg.c_str() + 9);
        }
    }

    oce::EngineConfig cfg;
    cfg.db_path = default_db_path();
    oce::Engine engine(cfg);

    auto app = oce::ui::UiApp::create("OpenCrawlEngine");
    if (!app) {
        std::fprintf(stderr, "OpenCrawlEngine: unable to open a window (no display?).\n");
        return 1;
    }

    oce::ui::GamePanels panels;
    int frames = 0;
    while (app->begin_frame()) {
        panels.draw(engine);
        app->end_frame();
        if (max_frames >= 0 && ++frames >= max_frames) {
            break;
        }
    }
    return 0;
}
