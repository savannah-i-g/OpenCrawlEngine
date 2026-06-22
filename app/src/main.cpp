// OpenCrawlEngine — application entry point.
//
// Builds the engine and UI, then runs the frame loop until the window closes.
// Pass --frames=N to render N frames and exit (used for automated smoke runs).

#include "oce/engine.hpp"
#include "oce_ui/asset_paths.hpp"
#include "oce_ui/game_panels.hpp"
#include "oce_ui/ui_app.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    int max_frames = -1; // negative: run until the window is closed
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--frames=", 0) == 0) {
            max_frames = std::atoi(arg.c_str() + 9);
        }
    }

    oce::EngineConfig cfg;
    cfg.db_path = (oce::ui::user_data_dir() / "oce.db").string();
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
