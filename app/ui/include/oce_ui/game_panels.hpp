#pragma once
// GamePanels — draws the game interface for one frame and dispatches user
// intent to the engine. Holds the transient widget state (input buffers).

#include "oce/engine.hpp"

#include <cstddef>
#include <string>
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
    // New Game world parameters (indices into the worldgen option lists).
    int  wp_biome_ = 0;
    int  wp_culture_ = 0;
    int  wp_population_ = 0;
    int  wp_technology_ = 0;
    int  wp_political_ = 0;
    int  wp_magic_ = 0;
    int  wp_species_ = 0;
    int  wp_threat_ = 0;
    char wp_custom_[256];
    bool awaiting_autofill_ = false;
    long long last_autofill_seq_ = 0;
    char model_buf_[128];
    char base_url_buf_[256];
    bool show_settings_ = false;
    bool show_new_game_ = false;
    bool show_assets_ = false;
    bool show_load_ = false;
    bool show_characters_ = false;
    bool show_campaigns_ = false;
    std::vector<oce::SaveInfo> saves_;
    std::vector<oce::SaveInfo> characters_;
    std::vector<oce::SaveInfo> campaigns_;
    std::string selected_char_;
    char camp_name_[64];
    char camp_theme_[128];
    char camp_tone_[64];
    char camp_goals_[256];
    char camp_custom_[512];
    int  camp_difficulty_ = 1; // Normal
};

} // namespace oce::ui
