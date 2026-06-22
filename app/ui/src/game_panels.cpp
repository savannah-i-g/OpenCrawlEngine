#include "oce_ui/game_panels.hpp"

#include "oce/engine.hpp"
#include "oce/rules/character.hpp"
#include "oce/rules/world.hpp"

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

ImVec4 rarity_color(oce::ItemRarity r) {
    switch (r) {
        case oce::ItemRarity::Common:
            return ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
        case oce::ItemRarity::Uncommon:
            return ImVec4(0.40f, 0.85f, 0.40f, 1.0f);
        case oce::ItemRarity::Rare:
            return ImVec4(0.40f, 0.60f, 1.00f, 1.0f);
        case oce::ItemRarity::Epic:
            return ImVec4(0.75f, 0.45f, 0.95f, 1.0f);
        case oce::ItemRarity::Legendary:
            return ImVec4(1.00f, 0.65f, 0.20f, 1.0f);
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

const char* rarity_label(oce::ItemRarity r) {
    switch (r) {
        case oce::ItemRarity::Common:
            return "Common";
        case oce::ItemRarity::Uncommon:
            return "Uncommon";
        case oce::ItemRarity::Rare:
            return "Rare";
        case oce::ItemRarity::Epic:
            return "Epic";
        case oce::ItemRarity::Legendary:
            return "Legendary";
    }
    return "Common";
}

const char* kind_label(oce::ItemKind k) {
    switch (k) {
        case oce::ItemKind::Weapon:
            return "weapon";
        case oce::ItemKind::Armor:
            return "armor";
        case oce::ItemKind::Potion:
            return "potion";
    }
    return "item";
}

std::string effects_summary(const oce::ItemEffects& e) {
    std::string out;
    auto add = [&out](const char* label, int v) {
        if (v == 0) {
            return;
        }
        if (!out.empty()) {
            out += ", ";
        }
        out += label;
        out += (v > 0) ? " +" : " ";
        out += std::to_string(v);
    };
    add("STR", e.strength);
    add("DEX", e.dexterity);
    add("INT", e.intelligence);
    add("CON", e.constitution);
    add("WIS", e.wisdom);
    add("CHA", e.charisma);
    add("LCK", e.luck);
    add("PER", e.perception);
    add("STL", e.stealth);
    add("BAR", e.bartering);
    add("HP", e.hp);
    add("EN", e.energy);
    return out;
}

// A lightweight rolling-die indicator shown while the engine resolves dice.
void dice_roll_indicator(const char* prefix) {
    const long ticks = static_cast<long>(ImGui::GetTime() * 10.0);
    const int face = static_cast<int>(((ticks % 6) + 6) % 6) + 1;
    ImGui::TextDisabled("%s rolling… [%d]", prefix, face);
}

// Renders narrative text with lightweight inline markup: words inside *...* or
// **...** are drawn in a highlight colour. The renderer word-wraps manually so
// the emphasis colour can change between words. Markup is display-only; the text
// is never interactive.
struct MarkdownToken {
    std::string text;
    bool emphasis;
};

void render_markdown(const std::string& text, const ImVec4& base) {
    const ImVec4 emph_color(1.0f, 0.86f, 0.55f, 1.0f);
    std::vector<MarkdownToken> tokens;
    std::string word;
    bool emphasis = false;
    bool word_emphasis = false;
    bool word_open = false;
    const size_t n = text.size();
    for (size_t i = 0; i < n;) {
        const char c = text[i];
        if (c == '*') {
            size_t j = i;
            while (j < n && text[j] == '*') {
                ++j;
            }
            emphasis = !emphasis; // any run of asterisks toggles emphasis
            i = j;
            continue;
        }
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
            if (word_open) {
                tokens.push_back({word, word_emphasis});
                word.clear();
                word_open = false;
            }
            if (c == '\n') {
                tokens.push_back({std::string("\n"), false});
            }
            ++i;
            continue;
        }
        if (!word_open) {
            word_open = true;
            word_emphasis = emphasis;
        }
        word += c;
        ++i;
    }
    if (word_open) {
        tokens.push_back({word, word_emphasis});
    }

    const float content_start = ImGui::GetCursorPosX();
    const float right = content_start + ImGui::GetContentRegionAvail().x;
    const float space_w = ImGui::CalcTextSize(" ").x;
    float cursor_x = content_start;
    bool line_start = true;
    for (const MarkdownToken& t : tokens) {
        if (t.text == "\n") {
            line_start = true;
            cursor_x = content_start;
            continue;
        }
        const float w = ImGui::CalcTextSize(t.text.c_str()).x;
        const bool wrap = !line_start && (cursor_x + space_w + w > right);
        if (!line_start && !wrap) {
            ImGui::SameLine(0.0f, space_w);
            cursor_x += space_w + w;
        } else {
            cursor_x = content_start + w; // a fresh Text starts a new line
        }
        ImGui::TextColored(t.emphasis ? emph_color : base, "%s", t.text.c_str());
        line_start = false;
    }
}

// Applies one of a few built-in palettes to the current ImGui style.
void apply_theme(const std::string& name) {
    ImGuiStyle& style = ImGui::GetStyle();
    if (name == "Parchment") {
        ImGui::StyleColorsLight();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.93f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.93f, 0.89f, 0.79f, 1.0f);
        style.Colors[ImGuiCol_Text] = ImVec4(0.20f, 0.16f, 0.11f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.82f, 0.74f, 0.58f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.80f, 0.71f, 0.54f, 1.0f);
    } else if (name == "Dusk") {
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.10f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.30f, 0.20f, 0.40f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.34f, 0.26f, 0.46f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.34f, 0.26f, 0.46f, 1.0f);
    } else { // Slate (default)
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.0f);
    }
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

const char* const kThemes[] = {"Slate", "Parchment", "Dusk"};

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
    apply_theme(s.theme);

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
        ImGui::TextDisabled("%s · %s", s.meta.name.c_str(),
                            oce::difficulty_to_string(s.meta.difficulty));
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

    // Inventory and equipment.
    if (ImGui::Begin("Inventory")) {
        ImGui::TextDisabled("Equipped");
        if (s.equipment.hand.has_value()) {
            ImGui::TextColored(rarity_color(s.equipment.hand->rarity), "Hand: %s",
                               s.equipment.hand->name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Unequip##hand")) {
                engine.player_unequip("hand");
            }
        } else {
            ImGui::TextDisabled("Hand: (empty)");
        }
        if (s.equipment.body.has_value()) {
            ImGui::TextColored(rarity_color(s.equipment.body->rarity), "Body: %s",
                               s.equipment.body->name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Unequip##body")) {
                engine.player_unequip("body");
            }
        } else {
            ImGui::TextDisabled("Body: (empty)");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Backpack (%zu)", s.inventory.size());
        if (ImGui::BeginChild("backpack", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            for (size_t i = 0; i < s.inventory.size(); ++i) {
                const oce::Item& it = s.inventory[i];
                ImGui::PushID((int) i);
                ImGui::TextColored(rarity_color(it.rarity), "%s", it.name.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s %s", rarity_label(it.rarity), kind_label(it.kind));
                    if (!it.description.empty()) {
                        ImGui::TextWrapped("%s", it.description.c_str());
                    }
                    const std::string eff = effects_summary(it.effects);
                    if (!eff.empty()) {
                        ImGui::TextUnformatted(eff.c_str());
                    }
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if (it.kind == oce::ItemKind::Potion) {
                    if (ImGui::SmallButton("Use")) {
                        engine.player_consume(it.id);
                    }
                } else if (ImGui::SmallButton("Equip")) {
                    engine.player_equip(it.id);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // Attributes.
    if (ImGui::Begin("Attributes")) {
        if (s.player.attribute_points > 0) {
            ImGui::Text("Points available: %d", s.player.attribute_points);
        } else {
            ImGui::TextDisabled("No points to allocate");
        }
        ImGui::Separator();
        const oce::Attributes& a = s.player.attributes;
        const bool can = s.player.attribute_points > 0 && !s.turn_in_progress;
        auto row = [&](const char* label, const char* key, int value) {
            ImGui::Text("%-13s %2d", label, value);
            if (can) {
                ImGui::SameLine();
                ImGui::PushID(key);
                if (ImGui::SmallButton("+")) {
                    engine.allocate_attribute(key);
                }
                ImGui::PopID();
            }
        };
        row("Strength", "strength", a.strength);
        row("Dexterity", "dexterity", a.dexterity);
        row("Intelligence", "intelligence", a.intelligence);
        row("Constitution", "constitution", a.constitution);
        row("Wisdom", "wisdom", a.wisdom);
        row("Charisma", "charisma", a.charisma);
        row("Luck", "luck", a.luck);
        row("Perception", "perception", a.perception);
        row("Stealth", "stealth", a.stealth);
        row("Bartering", "bartering", a.bartering);
    }
    ImGui::End();

    // Story log + command input.
    if (ImGui::Begin("Story")) {
        const float footer = ImGui::GetFrameHeightWithSpacing() * 3.0f;
        if (ImGui::BeginChild("log", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
            const ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            for (const oce::Message& m : s.story) {
                if (m.sender == "player") {
                    render_markdown("> " + m.content, ImVec4(0.60f, 0.80f, 1.0f, 1.0f));
                } else if (m.sender == "system") {
                    render_markdown(m.content, ImVec4(0.62f, 0.62f, 0.68f, 1.0f));
                } else {
                    render_markdown(m.content, text_col);
                }
                ImGui::Spacing();
            }
            if (!s.streaming_text.empty()) {
                render_markdown(s.streaming_text, text_col);
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        if (!s.status.empty()) {
            ImGui::TextDisabled("%s", s.status.c_str());
        }

        // Context-sensitive suggested actions as a wrapped row of small buttons.
        if (!s.suggested_actions.empty()) {
            ImGui::TextDisabled("Suggested:");
            const ImGuiStyle& style = ImGui::GetStyle();
            const float avail = ImGui::GetContentRegionAvail().x;
            float used = 0.0f;
            ImGui::BeginDisabled(s.turn_in_progress);
            for (size_t i = 0; i < s.suggested_actions.size(); ++i) {
                const std::string& action = s.suggested_actions[i];
                const float w = ImGui::CalcTextSize(action.c_str()).x + style.FramePadding.x * 2.0f;
                if (i > 0 && used + style.ItemSpacing.x + w <= avail) {
                    ImGui::SameLine();
                    used += style.ItemSpacing.x + w;
                } else {
                    used = w;
                }
                ImGui::PushID((int) i);
                if (ImGui::SmallButton(action.c_str())) {
                    engine.submit_turn(action);
                }
                ImGui::PopID();
            }
            ImGui::EndDisabled();
        }

        ImGui::BeginDisabled(s.turn_in_progress);
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
            const oce::Item* weapon =
                s.equipment.hand.has_value() ? &s.equipment.hand.value() : nullptr;
            const oce::Item* armor =
                s.equipment.body.has_value() ? &s.equipment.body.value() : nullptr;
            ImGui::Text("Attack +%d    Defense %d", oce::player_attack_bonus(s.player, weapon),
                        oce::player_defense(s.player, armor));
            ImGui::Text("HP %d/%d", s.player.hp, s.player.max_hp);
            if (s.turn_in_progress) {
                dice_roll_indicator("Enemies act —");
            } else {
                ImGui::TextDisabled("Your move.");
            }
            ImGui::Separator();

            ImGui::BeginDisabled(s.turn_in_progress);
            for (size_t i = 0; i < s.combat.enemies.size(); ++i) {
                const oce::Enemy& e = s.combat.enemies[i];
                ImGui::Text("%s  HP %d/%d", e.name.c_str(), e.hp, e.max_hp);
                ImGui::SameLine();
                ImGui::PushID((int) i);
                if (ImGui::SmallButton("Attack")) {
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
            // Use a potion as this turn's action.
            for (const oce::Item& it : s.inventory) {
                if (it.kind != oce::ItemKind::Potion) {
                    continue;
                }
                ImGui::PushID(it.id.c_str());
                if (ImGui::SmallButton(("Use " + it.name).c_str())) {
                    engine.combat_use_item(it.id);
                }
                ImGui::PopID();
            }
            ImGui::EndDisabled();

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
            ImGui::BeginDisabled(s.turn_in_progress);
            if (ImGui::Button("Roll")) {
                engine.resolve_skill_check();
            }
            ImGui::EndDisabled();
            if (s.turn_in_progress) {
                ImGui::SameLine();
                dice_roll_indicator("");
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
            if (ImGui::CollapsingHeader("Businesses", ImGuiTreeNodeFlags_DefaultOpen)) {
                int pending = 0;
                for (const oce::Business& b : s.assets.businesses) {
                    pending += oce::pending_business_income(b, s.world_state.time_elapsed);
                }
                ImGui::BeginDisabled(s.turn_in_progress || pending <= 0);
                if (ImGui::Button("Collect income")) {
                    engine.collect_income();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextDisabled("accrued: %d gold", pending);
                for (const oce::Business& b : s.assets.businesses) {
                    ImGui::BulletText("%s — %d gold/day", b.name.c_str(), b.income_per_day);
                }
                if (s.assets.businesses.empty()) {
                    ImGui::TextDisabled("None yet.");
                }
            }
            if (ImGui::CollapsingHeader("Relations")) {
                for (const oce::Relation& r : s.assets.relations) {
                    ImGui::BulletText("%s (%s, %d)", r.npc_name.c_str(), r.type.c_str(), r.strength);
                    ImGui::Indent();
                    if (r.strength >= 25) {
                        for (const std::string& ben : r.benefits) {
                            ImGui::TextDisabled("• %s", ben.c_str());
                        }
                    } else if (!r.benefits.empty()) {
                        ImGui::TextDisabled("(benefits unlock as the bond strengthens)");
                    }
                    ImGui::Unindent();
                }
            }
            if (ImGui::CollapsingHeader("Properties")) {
                for (const oce::Property& p : s.assets.properties) {
                    ImGui::BulletText("%s (%s)", p.name.c_str(), p.type.c_str());
                }
            }
            if (ImGui::CollapsingHeader("Mounts", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::BeginDisabled(s.turn_in_progress);
                if (ImGui::Button("Acquire a mount")) {
                    engine.acquire_mount();
                }
                ImGui::EndDisabled();
                if (s.turn_in_progress) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("Working…");
                }
                for (const oce::MountVehicle& m : s.assets.mounts) {
                    ImGui::BulletText("%s (%s) — condition %d", m.name.c_str(), m.type.c_str(),
                                      m.condition);
                    if (ImGui::IsItemHovered() && !m.description.empty()) {
                        ImGui::SetTooltip("%s", m.description.c_str());
                    }
                }
                if (s.assets.mounts.empty()) {
                    ImGui::TextDisabled("None yet.");
                }
            }
            if (ImGui::CollapsingHeader("Factions")) {
                for (const auto& kv : s.world_state.factions) {
                    const oce::Faction& f = kv.second;
                    ImGui::BulletText("%s — standing %d, reputation %d", f.name.c_str(),
                                      f.relationship, f.reputation);
                    ImGui::Indent();
                    if (f.relationship >= 25) {
                        for (const std::string& ben : f.benefits) {
                            ImGui::TextDisabled("• %s", ben.c_str());
                        }
                    } else if (!f.benefits.empty()) {
                        ImGui::TextDisabled("(benefits unlock at standing 25+)");
                    }
                    ImGui::Unindent();
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
            ImGui::Separator();
            int theme_index = 0;
            for (int i = 0; i < (int) (sizeof kThemes / sizeof kThemes[0]); ++i) {
                if (s.theme == kThemes[i]) {
                    theme_index = i;
                }
            }
            if (ImGui::Combo("Theme", &theme_index, kThemes,
                             (int) (sizeof kThemes / sizeof kThemes[0]))) {
                engine.set_theme(kThemes[theme_index]);
            }

            if (s.total_tokens > 0) {
                ImGui::TextDisabled("Tokens used this session: %lld", s.total_tokens);
            }
        }
        ImGui::End();
    }
}

} // namespace oce::ui
