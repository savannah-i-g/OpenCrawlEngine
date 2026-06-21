// OpenCrawlEngine — application entry point.
//
// Constructs the UI, then runs the frame loop until the window is closed.
// Pass --frames=N to render N frames and exit (used for automated smoke runs).

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

    auto app = oce::ui::UiApp::create("OpenCrawlEngine");
    if (!app) {
        std::fprintf(stderr, "OpenCrawlEngine: unable to open a window (no display?).\n");
        return 1;
    }

    int frames = 0;
    while (app->run_frame()) {
        if (max_frames >= 0 && ++frames >= max_frames) {
            break;
        }
    }
    return 0;
}
