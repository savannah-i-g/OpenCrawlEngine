#pragma once
// GamePanels — draws the game interface for one frame and dispatches user
// intent to the engine. Holds the transient widget state (input buffers).

#include <cstddef>

namespace oce {
class Engine;
}

namespace oce::ui {

class GamePanels {
public:
    GamePanels();
    void draw(oce::Engine& engine);

private:
    char input_[1024];
    char api_key_[256];
    bool show_settings_ = false;
};

} // namespace oce::ui
