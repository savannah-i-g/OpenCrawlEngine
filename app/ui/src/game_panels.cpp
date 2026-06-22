#include "oce_ui/game_panels.hpp"

#include "oce/engine.hpp"
#include "oce/rules/character.hpp"

#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace oce::ui {

namespace {

// Splits a comma-separated field into trimmed, non-empty entries.
std::vector<std::string> split_csv(const char* text) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        size_t b = cur.find_first_not_of(" \t");
        size_t e = cur.find_last_not_of(" \t");
        if (b != std::string::npos) {
            out.push_back(cur.substr(b, e - b + 1));
        }
        cur.clear();
    };
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == ',') {
            flush();
        } else {
            cur += *p;
        }
    }
    flush();
    return out;
}

oce::Difficulty difficulty_by_index(int index) {
    switch (index) {
        case 0:
            return oce::Difficulty::Easy;
        case 2:
            return oce::Difficulty::Hard;
        case 3:
            return oce::Difficulty::Deadly;
        default:
            return oce::Difficulty::Normal;
    }
}

} // namespace

GamePanels::GamePanels() {
    input_[0] = '\0';
    api_key_[0] = '\0';
    new_background_[0] = '\0';
    new_world_[0] = '\0';
    model_buf_[0] = '\0';
    base_url_buf_[0] = '\0';
    camp_theme_[0] = '\0';
    camp_tone_[0] = '\0';
    camp_goals_[0] = '\0';
    camp_custom_[0] = '\0';
    wp_custom_[0] = '\0';
    std::snprintf(new_name_, sizeof new_name_, "%s", "Adventurer");
    std::snprintf(camp_name_, sizeof camp_name_, "%s", "Adventure");
}

void GamePanels::draw(oce::Engine& engine) {
    const oce::Snapshot s = engine.snapshot();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Game")) {
            if (ImGui::MenuItem("New Game...")) {
                show_new_game_ = true;
            }
            if (ImGui::MenuItem("Characters...")) {
                show_characters_ = true;
                characters_ = engine.list_characters();
            }
            if (ImGui::MenuItem("Load Game...")) {
                show_load_ = true;
                saves_ = engine.list_saves();
            }
            if (ImGui::MenuItem("Assets...")) {
                show_assets_ = true;
            }
            if (ImGui::MenuItem("Settings...")) {
                show_settings_ = true;
                std::snprintf(model_buf_, sizeof model_buf_, "%s", s.model.c_str());
                std::snprintf(base_url_buf_, sizeof base_url_buf_, "%s", s.base_url.c_str());
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Character status.
    if (ImGui::Begin("Character")) {
        ImGui::Text("%s", s.player.name.c_str());
        ImGui::TextDisabled("Level %d %s", s.player.level, class_to_string(s.player.cls));
        ImGui::Spacing();

        const float hp_frac =
            s.player.max_hp > 0 ? (float) s.player.hp / (float) s.player.max_hp : 0.0f;
        ImGui::ProgressBar(hp_frac, ImVec2(-1.0f, 0.0f));
        ImGui::Text("HP %d / %d", s.player.hp, s.player.max_hp);

        const float energy_frac =
            s.player.max_energy > 0 ? (float) s.player.energy / (float) s.player.max_energy : 0.0f;
        ImGui::ProgressBar(energy_frac, ImVec2(-1.0f, 0.0f));
        ImGui::Text("Energy %d / %d", s.player.energy, s.player.max_energy);

        ImGui::Spacing();
        ImGui::Text("Gold: %d", s.player.gold);
        ImGui::TextDisabled("XP: %lld", s.player.xp);
        if (s.total_tokens > 0) {
            ImGui::TextDisabled("Tokens used: %lld", s.total_tokens);
        }
    }
    ImGui::End();

    // Story log + command input.
    if (ImGui::Begin("Story")) {
        const float footer = ImGui::GetFrameHeightWithSpacing() * 3.0f;
        if (ImGui::BeginChild("log", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
            for (const oce::Message& m : s.story) {
                if (m.sender == "player") {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                    ImGui::TextWrapped("> %s", m.content.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextWrapped("%s", m.content.c_str());
                }
                ImGui::Spacing();
            }
            if (!s.streaming_text.empty()) {
                ImGui::TextWrapped("%s", s.streaming_text.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        if (!s.status.empty()) {
            ImGui::TextDisabled("%s", s.status.c_str());
        }

        ImGui::BeginDisabled(s.turn_in_progress);
        for (size_t i = 0; i < s.suggested_actions.size(); ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            ImGui::PushID((int) i);
            if (ImGui::Button(s.suggested_actions[i].c_str())) {
                engine.submit_turn(s.suggested_actions[i]);
            }
            ImGui::PopID();
        }

        bool enter = ImGui::InputText("##input", input_, sizeof input_,
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool send = ImGui::Button("Send");
        ImGui::EndDisabled();

        if ((enter || send) && input_[0] != '\0' && !s.turn_in_progress) {
            engine.submit_turn(input_);
            input_[0] = '\0';
        }
        if (s.turn_in_progress) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                engine.cancel_turn();
            }
        }
    }
    ImGui::End();

    // Combat.
    if (s.combat.active) {
        if (ImGui::Begin("Combat")) {
            for (size_t i = 0; i < s.combat.enemies.size(); ++i) {
                const oce::Enemy& e = s.combat.enemies[i];
                ImGui::Text("%s  HP %d/%d", e.name.c_str(), e.hp, e.max_hp);
                ImGui::SameLine();
                ImGui::PushID((int) i);
                if (ImGui::Button("Attack")) {
                    engine.combat_action("attack", (int) i);
                }
                ImGui::PopID();
            }
            ImGui::Spacing();
            if (ImGui::Button("Defend")) {
                engine.combat_action("defend", 0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Flee")) {
                engine.combat_action("flee", 0);
            }
            ImGui::Separator();
            if (ImGui::BeginChild("combat_log", ImVec2(0.0f, 160.0f), ImGuiChildFlags_Borders)) {
                for (const std::string& line : s.combat.log) {
                    ImGui::TextWrapped("%s", line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    // Skill check.
    if (s.skill_check.active) {
        if (ImGui::Begin("Skill Check")) {
            ImGui::TextWrapped("%s", s.skill_check.description.empty()
                                         ? "A test of skill."
                                         : s.skill_check.description.c_str());
            ImGui::Text("%s vs difficulty %d  (%dd6)", s.skill_check.attribute.c_str(),
                        s.skill_check.difficulty, s.skill_check.num_dice);
            if (ImGui::Button("Roll")) {
                engine.resolve_skill_check();
            }
        }
        ImGui::End();
    }

    // New game.
    if (show_new_game_) {
        if (ImGui::Begin("New Game", &show_new_game_)) {
            ImGui::InputText("Name", new_name_, sizeof new_name_);
            const char* classes[] = {"Warrior", "Rogue", "Mage", "Cleric", "Ranger", "Bard"};
            ImGui::Combo("Class", &new_class_, classes, 6);
            ImGui::InputTextMultiline("Background", new_background_, sizeof new_background_,
                                      ImVec2(0.0f, 50.0f));

            ImGui::SeparatorText("World");
            ImGui::Combo("Biome", &wp_biome_, oce::BIOME_OPTIONS.data(),
                         (int) oce::BIOME_OPTIONS.size());
            ImGui::Combo("Culture", &wp_culture_, oce::CULTURE_OPTIONS.data(),
                         (int) oce::CULTURE_OPTIONS.size());
            ImGui::Combo("Population", &wp_population_, oce::POPULATION_OPTIONS.data(),
                         (int) oce::POPULATION_OPTIONS.size());
            ImGui::Combo("Technology", &wp_technology_, oce::TECHNOLOGY_OPTIONS.data(),
                         (int) oce::TECHNOLOGY_OPTIONS.size());
            ImGui::Combo("Politics", &wp_political_, oce::POLITICAL_OPTIONS.data(),
                         (int) oce::POLITICAL_OPTIONS.size());
            ImGui::Combo("Magic", &wp_magic_, oce::MAGIC_OPTIONS.data(),
                         (int) oce::MAGIC_OPTIONS.size());
            ImGui::Combo("Species", &wp_species_, oce::SPECIES_OPTIONS.data(),
                         (int) oce::SPECIES_OPTIONS.size());
            ImGui::Combo("Threat", &wp_threat_, oce::THREAT_OPTIONS.data(),
                         (int) oce::THREAT_OPTIONS.size());
            ImGui::InputText("Custom notes", wp_custom_, sizeof wp_custom_);
            ImGui::InputTextMultiline("Premise", new_world_, sizeof new_world_, ImVec2(0.0f, 50.0f));

            auto pick = [](const auto& arr, int i) -> std::string {
                return (i >= 0 && (size_t) i < arr.size()) ? std::string(arr[(size_t) i])
                                                           : std::string();
            };
            auto make_params = [&]() {
                oce::WorldParams wp;
                wp.biome = pick(oce::BIOME_OPTIONS, wp_biome_);
                wp.culture = pick(oce::CULTURE_OPTIONS, wp_culture_);
                wp.population = pick(oce::POPULATION_OPTIONS, wp_population_);
                wp.technology = pick(oce::TECHNOLOGY_OPTIONS, wp_technology_);
                wp.political = pick(oce::POLITICAL_OPTIONS, wp_political_);
                wp.magic = pick(oce::MAGIC_OPTIONS, wp_magic_);
                wp.species = pick(oce::SPECIES_OPTIONS, wp_species_);
                wp.threat = pick(oce::THREAT_OPTIONS, wp_threat_);
                wp.custom_fields = split_csv(wp_custom_);
                if (new_world_[0] != '\0') {
                    wp.custom_fields.emplace_back(new_world_);
                }
                return wp;
            };

            if (ImGui::Button("Suggest premise")) {
                awaiting_autofill_ = true;
                last_autofill_seq_ = s.autofill_seq;
                engine.request_autofill(make_params(), "premise");
            }
            if (awaiting_autofill_ && s.autofill_seq > last_autofill_seq_) {
                std::snprintf(new_world_, sizeof new_world_, "%s", s.autofill_value.c_str());
                awaiting_autofill_ = false;
            }

            oce::NewGameParams p;
            p.name = (new_name_[0] != '\0') ? new_name_ : "Adventurer";
            const oce::CharacterClass by_index[] = {
                oce::CharacterClass::Warrior, oce::CharacterClass::Rogue,
                oce::CharacterClass::Mage,    oce::CharacterClass::Cleric,
                oce::CharacterClass::Ranger,  oce::CharacterClass::Bard};
            const int idx = (new_class_ >= 0 && new_class_ < 6) ? new_class_ : 0;
            p.cls = by_index[idx];
            p.background = new_background_;
            p.world_prompt = new_world_;

            ImGui::Separator();
            ImGui::BeginDisabled(s.turn_in_progress);
            if (ImGui::Button("Quick Start")) {
                engine.new_game(p);
                show_new_game_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Generate World")) {
                engine.new_game(p);
                engine.generate_world(make_params());
                show_new_game_ = false;
            }
            ImGui::EndDisabled();
            if (s.turn_in_progress) {
                ImGui::SameLine();
                ImGui::TextDisabled("Working…");
            }
        }
        ImGui::End();
    }

    // Assets.
    if (show_assets_) {
        if (ImGui::Begin("Assets", &show_assets_)) {
            if (ImGui::CollapsingHeader("Businesses")) {
                for (const oce::Business& b : s.assets.businesses) {
                    ImGui::BulletText("%s — %d gold/day", b.name.c_str(), b.income_per_day);
                }
            }
            if (ImGui::CollapsingHeader("Relations")) {
                for (const oce::Relation& r : s.assets.relations) {
                    ImGui::BulletText("%s (%s, %d)", r.npc_name.c_str(), r.type.c_str(), r.strength);
                }
            }
            if (ImGui::CollapsingHeader("Properties")) {
                for (const oce::Property& p : s.assets.properties) {
                    ImGui::BulletText("%s (%s)", p.name.c_str(), p.type.c_str());
                }
            }
            if (ImGui::CollapsingHeader("Mounts")) {
                for (const oce::MountVehicle& m : s.assets.mounts) {
                    ImGui::BulletText("%s (%s) — condition %d", m.name.c_str(), m.type.c_str(),
                                      m.condition);
                }
            }
            if (ImGui::CollapsingHeader("Factions")) {
                for (const auto& kv : s.world_state.factions) {
                    const oce::Faction& f = kv.second;
                    ImGui::BulletText("%s — standing %d, reputation %d", f.name.c_str(),
                                      f.relationship, f.reputation);
                }
            }
            if (ImGui::CollapsingHeader("Known NPCs")) {
                for (const auto& kv : s.world_state.known_npcs) {
                    const oce::NPC& n = kv.second;
                    ImGui::BulletText("%s — %s", n.name.c_str(), n.location.c_str());
                }
            }
        }
        ImGui::End();
    }

    // Load game.
    if (show_load_) {
        if (ImGui::Begin("Load Game", &show_load_)) {
            if (ImGui::Button("Refresh")) {
                saves_ = engine.list_saves();
            }
            ImGui::Separator();
            if (saves_.empty()) {
                ImGui::TextDisabled("No saved games yet.");
            }
            for (size_t i = 0; i < saves_.size(); ++i) {
                ImGui::PushID((int) i);
                if (ImGui::Button("Load")) {
                    engine.load_save(saves_[i].id);
                    show_load_ = false;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(saves_[i].label.c_str());
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    // Character selection.
    if (show_characters_) {
        if (ImGui::Begin("Characters", &show_characters_)) {
            if (ImGui::Button("Refresh")) {
                characters_ = engine.list_characters();
            }
            ImGui::SameLine();
            if (ImGui::Button("New Character...")) {
                show_new_game_ = true;
            }
            ImGui::Separator();
            if (characters_.empty()) {
                ImGui::TextDisabled("No characters yet. Create one with New Character.");
            }
            for (size_t i = 0; i < characters_.size(); ++i) {
                ImGui::PushID((int) i);
                ImGui::TextUnformatted(characters_[i].label.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Campaigns")) {
                    selected_char_ = characters_[i].id;
                    campaigns_ = engine.list_campaigns(selected_char_);
                    show_campaigns_ = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    engine.delete_character(characters_[i].id);
                    characters_ = engine.list_characters();
                    ImGui::PopID();
                    break; // list changed; redraw next frame
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    // Campaign manager for the selected character.
    if (show_campaigns_) {
        if (ImGui::Begin("Campaign Manager", &show_campaigns_)) {
            if (ImGui::Button("Refresh")) {
                campaigns_ = engine.list_campaigns(selected_char_);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Campaigns");
            if (campaigns_.empty()) {
                ImGui::TextDisabled("None yet — create one below.");
            }
            for (size_t i = 0; i < campaigns_.size(); ++i) {
                ImGui::PushID((int) i);
                if (ImGui::SmallButton("Play")) {
                    engine.load_save(campaigns_[i].id);
                    show_campaigns_ = false;
                    show_characters_ = false;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    engine.delete_campaign(campaigns_[i].id);
                    campaigns_ = engine.list_campaigns(selected_char_);
                    ImGui::PopID();
                    break; // list changed; redraw next frame
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(campaigns_[i].label.c_str());
                ImGui::PopID();
            }
            ImGui::Separator();
            ImGui::TextDisabled("New campaign");
            ImGui::InputText("Name##camp", camp_name_, sizeof camp_name_);
            ImGui::InputText("Theme##camp", camp_theme_, sizeof camp_theme_);
            ImGui::InputText("Tone##camp", camp_tone_, sizeof camp_tone_);
            ImGui::InputText("Goals (comma-separated)##camp", camp_goals_, sizeof camp_goals_);
            const char* diffs[] = {"Easy", "Normal", "Hard", "Deadly"};
            ImGui::Combo("Difficulty##camp", &camp_difficulty_, diffs, 4);
            ImGui::InputTextMultiline("Custom prompt##camp", camp_custom_, sizeof camp_custom_,
                                      ImVec2(0.0f, 60.0f));
            ImGui::BeginDisabled(selected_char_.empty());
            if (ImGui::Button("Create campaign")) {
                oce::CampaignParams cp;
                cp.name = (camp_name_[0] != '\0') ? camp_name_ : "Adventure";
                cp.theme = camp_theme_;
                cp.tone = camp_tone_;
                cp.custom_prompt = camp_custom_;
                cp.difficulty = difficulty_by_index(camp_difficulty_);
                cp.goals = split_csv(camp_goals_);
                engine.new_campaign(selected_char_, cp);
                show_campaigns_ = false;
                show_characters_ = false;
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

    // Settings.
    if (show_settings_) {
        if (ImGui::Begin("Settings", &show_settings_)) {
            ImGui::TextWrapped("Enter your OpenRouter API key. It is held in memory only and is "
                               "never written to disk.");
            ImGui::InputText("API key", api_key_, sizeof api_key_, ImGuiInputTextFlags_Password);
            if (ImGui::Button("Save key") && api_key_[0] != '\0') {
                engine.set_api_key(api_key_);
                std::memset(api_key_, 0, sizeof api_key_);
            }
            ImGui::Separator();
            ImGui::InputText("Model", model_buf_, sizeof model_buf_);
            ImGui::InputText("Base URL", base_url_buf_, sizeof base_url_buf_);
            if (ImGui::Button("Save provider")) {
                if (model_buf_[0] != '\0') {
                    engine.set_model(model_buf_);
                }
                if (base_url_buf_[0] != '\0') {
                    engine.set_base_url(base_url_buf_);
                }
            }
            if (s.total_tokens > 0) {
                ImGui::TextDisabled("Tokens used this session: %lld", s.total_tokens);
            }
        }
        ImGui::End();
    }
}

} // namespace oce::ui
