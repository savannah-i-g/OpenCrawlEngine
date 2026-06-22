#pragma once
// GamePanels — draws the game interface for one frame and dispatches user
// intent to the engine. Builds a docked HUD (top stat-bar, left character
// sidebar, central story + action row + input) and a set of modal dialogs.
// Holds transient widget state (input buffers, dialog flags) and the icon cache.

#include "oce_ui/icon_cache.hpp"

#include "oce/engine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oce::ui {

class GamePanels {
public:
    GamePanels();
    void draw(oce::Engine& engine);

private:
    void build_layout(oce::Engine& engine, const oce::Snapshot& s);
    void draw_modals(oce::Engine& engine, const oce::Snapshot& s);
    void setup_dock_layout(unsigned int dockspace_id);

    IconCache icons_;
    bool      reset_layout_ = true;

    char input_[1024];
    char api_key_[256];

    // New Game ("Forge Your Destiny") form.
    char new_name_[64];
    char new_background_[512];
    char new_world_[512];
    int  new_class_ = 0;
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

    // Settings.
    char model_buf_[128];
    char base_url_buf_[256];

    // Dialog visibility.
    bool show_settings_ = false;
    bool show_new_game_ = false;
    bool show_assets_ = false;
    bool show_load_ = false;
    bool show_characters_ = false;
    bool show_campaigns_ = false;
    bool show_inventory_ = false;
    bool show_stats_ = false;
    bool show_gm_ = false;

    std::vector<oce::SaveInfo> saves_;
    std::vector<oce::SaveInfo> characters_;
    std::vector<oce::SaveInfo> campaigns_;
    std::string selected_char_;

    // Campaign Manager — new campaign form.
    char camp_name_[64];
    char camp_theme_[128];
    char camp_tone_[64];
    char camp_goals_[256];
    char camp_custom_[512];
    int  camp_difficulty_ = 1; // Normal

    int combat_target_ = 0;
    bool show_skill_ = false;
    bool prev_skill_active_ = false;
    long long skill_base_seq_ = 0;

    // Game Master tools form.
    char gm_item_name_[64];
    int  gm_item_type_ = 0;
    int  gm_item_rarity_ = 0;
    int  gm_item_power_ = 3;
    int  gm_gold_ = 0;
    int  gm_xp_ = 0;
    char gm_faction_[64];
    int  gm_faction_rel_ = 10;
};

} // namespace oce::ui
