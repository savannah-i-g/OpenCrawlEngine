#include "oce_ui/game_panels.hpp"

#include "oce/engine.hpp"
#include "oce/rules/character.hpp"

#include "imgui.h"

#include <cstring>

namespace oce::ui {

GamePanels::GamePanels() {
    input_[0] = '\0';
    api_key_[0] = '\0';
}

void GamePanels::draw(oce::Engine& engine) {
    const oce::Snapshot s = engine.snapshot();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Game")) {
            if (ImGui::MenuItem("Settings...")) {
                show_settings_ = true;
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

    // Settings.
    if (show_settings_) {
        if (ImGui::Begin("Settings", &show_settings_)) {
            ImGui::TextWrapped("Enter your OpenRouter API key. It is held in memory only and is "
                               "never written to disk.");
            ImGui::InputText("API key", api_key_, sizeof api_key_, ImGuiInputTextFlags_Password);
            if (ImGui::Button("Save key") && api_key_[0] != '\0') {
                engine.set_api_key(api_key_);
                std::memset(api_key_, 0, sizeof api_key_);
                show_settings_ = false;
            }
        }
        ImGui::End();
    }
}

} // namespace oce::ui
