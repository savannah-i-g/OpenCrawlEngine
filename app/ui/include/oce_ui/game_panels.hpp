#pragma once
// GamePanels — draws the game interface for one frame and dispatches user
// intent to the engine. Holds the transient widget state (input buffers).

#include "oce/engine.hpp"

#include <cstddef>
#include <vector>

namespace oce::ui {

class GamePanels {
public:
    GamePanels();
    void draw(oce::Engine& engine);

private:
    char input_[1024];
    char api_key_[256];
    char new_name_[64];
    char new_background_[512];
    char new_world_[512];
    int  new_class_ = 0;
    char model_buf_[128];
    char base_url_buf_[256];
    bool show_settings_ = false;
    bool show_new_game_ = false;
    bool show_assets_ = false;
    bool show_load_ = false;
    std::vector<oce::SaveInfo> saves_;
};

} // namespace oce::ui
