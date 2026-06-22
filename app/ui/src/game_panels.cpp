#include "oce_ui/game_panels.hpp"

#include "oce_ui/ui_fonts.hpp"

#include "oce/engine.hpp"
#include "oce/rules/character.hpp"
#include "oce/rules/leveling.hpp"
#include "oce/rules/skills.hpp"
#include "oce/rules/world.hpp"

#include "imgui.h"
#include "imgui_internal.h" // DockBuilder

#include <cctype>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace oce::ui {

namespace {

// --- small helpers ---------------------------------------------------------

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

const char* kind_icon(oce::ItemKind k) {
    switch (k) {
        case oce::ItemKind::Weapon:
            return "broadsword";
        case oce::ItemKind::Armor:
            return "breastplate";
        case oce::ItemKind::Potion:
            return "potion-ball";
    }
    return "broadsword";
}

const char* class_icon(oce::CharacterClass c) {
    switch (c) {
        case oce::CharacterClass::Warrior:
            return "broadsword";
        case oce::CharacterClass::Rogue:
            return "hooded-assassin";
        case oce::CharacterClass::Mage:
            return "wizard-staff";
        case oce::CharacterClass::Cleric:
            return "angel-wings";
        case oce::CharacterClass::Ranger:
            return "high-shot";
        case oce::CharacterClass::Bard:
            return "lyre";
    }
    return "broadsword";
}

const char* sender_icon(const std::string& sender) {
    if (sender == "player") {
        return "crown";
    }
    if (sender == "system") {
        return "console-controller";
    }
    return "pointy-hat"; // narrator
}

const char* business_type_label(oce::BusinessType t) {
    switch (t) {
        case oce::BusinessType::Tavern:
            return "Tavern";
        case oce::BusinessType::Shop:
            return "Shop";
        case oce::BusinessType::Farm:
            return "Farm";
        case oce::BusinessType::Mine:
            return "Mine";
        case oce::BusinessType::TradingCompany:
            return "Trading Company";
        case oce::BusinessType::MercenaryGuild:
            return "Mercenary Guild";
        case oce::BusinessType::Workshop:
            return "Workshop";
        case oce::BusinessType::Other:
            return "Other";
    }
    return "Other";
}

const char* business_type_icon(oce::BusinessType t) {
    switch (t) {
        case oce::BusinessType::Tavern:
            return "campfire";
        case oce::BusinessType::Shop:
            return "shop";
        case oce::BusinessType::Farm:
            return "wheat";
        case oce::BusinessType::Mine:
            return "anvil";
        case oce::BusinessType::TradingCompany:
            return "cash";
        case oce::BusinessType::MercenaryGuild:
            return "crossed-swords";
        case oce::BusinessType::Workshop:
            return "anvil";
        case oce::BusinessType::Other:
            return "briefcase";
    }
    return "briefcase";
}

const char* faction_type_label(oce::FactionType t) {
    switch (t) {
        case oce::FactionType::Guild:
            return "Guild";
        case oce::FactionType::Kingdom:
            return "Kingdom";
        case oce::FactionType::Clan:
            return "Clan";
        case oce::FactionType::Cult:
            return "Cult";
        case oce::FactionType::MerchantCompany:
            return "Merchant Company";
        case oce::FactionType::Military:
            return "Military";
        case oce::FactionType::Religious:
            return "Religious";
        case oce::FactionType::Criminal:
            return "Criminal";
        case oce::FactionType::Other:
            return "Other";
    }
    return "Other";
}

const char* faction_type_icon(oce::FactionType t) {
    switch (t) {
        case oce::FactionType::Guild:
            return "briefcase";
        case oce::FactionType::Kingdom:
            return "castle";
        case oce::FactionType::Clan:
            return "swords-emblem";
        case oce::FactionType::Cult:
            return "pentacle";
        case oce::FactionType::MerchantCompany:
            return "coins-pile";
        case oce::FactionType::Military:
            return "crossed-swords";
        case oce::FactionType::Religious:
            return "temple-gate";
        case oce::FactionType::Criminal:
            return "hooded-figure";
        case oce::FactionType::Other:
            return "crossed-swords";
    }
    return "crossed-swords";
}

// A small framed "badge" label (e.g. a type tag on a card).
void badge(const char* text, const ImVec4& col) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::SmallButton(text);
    ImGui::PopStyleColor(2);
}

// A standing/relationship bar (-100..100) drawn as a thin colored meter.
void standing_bar(int value) {
    const float frac = (float) (value + 100) / 200.0f;
    const ImVec4 col = value >= 25    ? ImVec4(0.46f, 0.80f, 0.42f, 1.0f)
                       : value <= -25 ? ImVec4(0.85f, 0.34f, 0.31f, 1.0f)
                                      : ImVec4(0.70f, 0.66f, 0.45f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    ImGui::ProgressBar(frac, ImVec2(-1.0f, 10.0f), "");
    ImGui::PopStyleColor();
}

struct AttrInfo {
    const char* key;
    const char* label;
    const char* icon;
    ImVec4 color;
};

const AttrInfo kAttrs[] = {
    {"strength", "Strength", "fist", ImVec4(0.85f, 0.36f, 0.31f, 1)},
    {"dexterity", "Dexterity", "running-ninja", ImVec4(0.41f, 0.66f, 0.95f, 1)},
    {"intelligence", "Intelligence", "spell-book", ImVec4(0.70f, 0.46f, 0.90f, 1)},
    {"constitution", "Constitution", "health-increase", ImVec4(0.46f, 0.80f, 0.46f, 1)},
    {"wisdom", "Wisdom", "owl", ImVec4(0.40f, 0.80f, 0.85f, 1)},
    {"charisma", "Charisma", "public-speaker", ImVec4(0.95f, 0.56f, 0.76f, 1)},
    {"luck", "Luck", "dice-six-faces-five", ImVec4(0.90f, 0.80f, 0.36f, 1)},
    {"perception", "Perception", "spyglass", ImVec4(0.90f, 0.66f, 0.36f, 1)},
    {"stealth", "Stealth", "hood", ImVec4(0.62f, 0.64f, 0.68f, 1)},
    {"bartering", "Bartering", "pay-money", ImVec4(0.80f, 0.71f, 0.46f, 1)},
};

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

// A square-cornered stat bar with a horizontal dark→bright gradient fill, a
// themed border, and a centered value overlay. Advances the cursor like a
// widget of the given size.
void gradient_bar(float w, float h, float frac, const ImVec4& col, const char* overlay,
                  const ImVec4& border) {
    frac = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 end(p.x + w, p.y + h);
    dl->AddRectFilled(p, end, ImGui::GetColorU32(ImVec4(0.03f, 0.03f, 0.03f, 0.85f)), 0.0f);
    if (frac > 0.001f) {
        const ImVec2 fend(p.x + w * frac, end.y);
        const ImU32 dark =
            ImGui::GetColorU32(ImVec4(col.x * 0.45f, col.y * 0.45f, col.z * 0.45f, 1.0f));
        const ImU32 bright = ImGui::GetColorU32(col);
        dl->AddRectFilledMultiColor(p, fend, dark, bright, bright, dark);
    }
    dl->AddRect(p, end, ImGui::GetColorU32(border), 0.0f, 0, 1.5f);
    if (overlay != nullptr && overlay[0] != '\0') {
        const ImVec2 ts = ImGui::CalcTextSize(overlay);
        const ImVec2 tp(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f);
        dl->AddText(ImVec2(tp.x + 1.0f, tp.y + 1.0f), ImGui::GetColorU32(ImVec4(0, 0, 0, 0.55f)),
                    overlay);
        dl->AddText(tp, ImGui::GetColorU32(ImVec4(0.96f, 0.94f, 0.88f, 1.0f)), overlay);
    }
    ImGui::Dummy(ImVec2(w, h));
}

void heading(const char* text) {
    if (g_heading_font != nullptr) {
        ImGui::PushFont(g_heading_font);
    }
    ImGui::TextUnformatted(text);
    if (g_heading_font != nullptr) {
        ImGui::PopFont();
    }
}

struct ColorTag {
    const char* name;
    ImVec4 color;
};

const ColorTag kColorTags[] = {
    {"green", ImVec4(0.46f, 0.80f, 0.42f, 1.0f)},  {"red", ImVec4(0.87f, 0.34f, 0.31f, 1.0f)},
    {"blue", ImVec4(0.43f, 0.63f, 0.95f, 1.0f)},   {"gold", ImVec4(0.90f, 0.76f, 0.40f, 1.0f)},
    {"purple", ImVec4(0.72f, 0.50f, 0.93f, 1.0f)}, {"gray", ImVec4(0.64f, 0.62f, 0.56f, 1.0f)},
    {"grey", ImVec4(0.64f, 0.62f, 0.56f, 1.0f)},
};

bool color_for_tag(const std::string& name, ImVec4& out) {
    for (const ColorTag& t : kColorTags) {
        if (name == t.name) {
            out = t.color;
            return true;
        }
    }
    return false;
}

// Narrative with lightweight inline markup: **bold** (warm gold, bold serif),
// *italic* (italic serif), and <color>…</color> tints (green/red/blue/gold/
// purple/gray). Display-only, word-wrapped manually so styling varies per word.
void render_markdown(const std::string& text, const ImVec4& base) {
    const ImVec4 bold_color(0.90f, 0.76f, 0.42f, 1.0f);
    struct Token {
        std::string text;
        bool bold;
        bool italic;
        bool brk;
        bool has_color;
        ImVec4 color;
    };
    std::vector<Token> tokens;
    std::string word;
    bool word_open = false;
    bool word_bold = false;
    bool word_italic = false;
    bool word_has_color = false;
    ImVec4 word_color(1, 1, 1, 1);
    bool bold = false;
    bool italic = false;
    bool has_color = false;
    ImVec4 cur_color(1, 1, 1, 1);
    auto open_word = [&]() {
        if (!word_open) {
            word_open = true;
            word_bold = bold;
            word_italic = italic;
            word_has_color = has_color;
            word_color = cur_color;
        }
    };
    auto push_word = [&]() {
        if (word_open) {
            tokens.push_back({word, word_bold, word_italic, false, word_has_color, word_color});
            word.clear();
            word_open = false;
        }
    };
    const size_t n = text.size();
    for (size_t i = 0; i < n;) {
        const char c = text[i];
        if (c == '<') {
            const size_t j = text.find('>', i);
            if (j != std::string::npos) {
                const std::string inner = text.substr(i + 1, j - (i + 1));
                const bool close = !inner.empty() && inner[0] == '/';
                ImVec4 col;
                if (color_for_tag(close ? inner.substr(1) : inner, col)) {
                    push_word();
                    has_color = !close;
                    cur_color = col;
                    i = j + 1;
                    continue;
                }
            }
            open_word();
            word += c;
            ++i;
            continue;
        }
        if (c == '*' || c == '_') {
            size_t j = i;
            while (j < n && (text[j] == '*' || text[j] == '_')) {
                ++j;
            }
            push_word();
            if (j - i >= 2) {
                bold = !bold;
            } else {
                italic = !italic;
            }
            i = j;
            continue;
        }
        if (c == '\n') {
            push_word();
            tokens.push_back({std::string(), false, false, true, false, ImVec4(1, 1, 1, 1)});
            ++i;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            push_word();
            ++i;
            continue;
        }
        open_word();
        word += c;
        ++i;
    }
    push_word();

    const float content_start = ImGui::GetCursorPosX();
    const float right = content_start + ImGui::GetContentRegionAvail().x;
    const float space_w = ImGui::CalcTextSize(" ").x;
    float cursor_x = content_start;
    bool line_start = true;
    for (const Token& t : tokens) {
        if (t.brk) {
            line_start = true;
            cursor_x = content_start;
            continue;
        }
        ImFont* font = (t.bold && t.italic) ? g_bolditalic_font
                       : t.bold             ? g_bold_font
                       : t.italic           ? g_italic_font
                                            : nullptr;
        if (font != nullptr) {
            ImGui::PushFont(font);
        }
        const float w = ImGui::CalcTextSize(t.text.c_str()).x;
        const bool wrap = !line_start && (cursor_x + space_w + w > right);
        if (!line_start && !wrap) {
            ImGui::SameLine(0.0f, space_w);
            cursor_x += space_w + w;
        } else {
            cursor_x = content_start + w;
        }
        const ImVec4 color = t.has_color ? t.color : (t.bold ? bold_color : base);
        ImGui::TextColored(color, "%s", t.text.c_str());
        if (font != nullptr) {
            ImGui::PopFont();
        }
        line_start = false;
    }
}

const char* const kThemes[] = {"Gold", "Ember", "Arcane"};

ImVec4 accent_for(const std::string& name) {
    if (name == "Ember") {
        return ImVec4(0.86f, 0.43f, 0.22f, 1.0f);
    }
    if (name == "Arcane") {
        return ImVec4(0.56f, 0.50f, 0.95f, 1.0f);
    }
    return ImVec4(0.83f, 0.69f, 0.33f, 1.0f); // Gold (default)
}

// Dark-fantasy palette: near-black green-tinted ground, parchment text, a single
// accent for interactive slots. StyleColorsDark is the base; we override on top.
void apply_theme(const std::string& name) {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    const ImVec4 acc = accent_for(name);
    const ImVec4 bg = ImVec4(0.07f, 0.075f, 0.066f, 1.0f);
    const ImVec4 panel = ImVec4(0.105f, 0.110f, 0.095f, 1.0f);
    const ImVec4 input = ImVec4(0.05f, 0.055f, 0.05f, 1.0f);
    const ImVec4 header = ImVec4(0.16f, 0.15f, 0.11f, 1.0f);
    const ImVec4 hover = ImVec4(0.23f, 0.21f, 0.14f, 1.0f);
    const ImVec4 text = ImVec4(0.89f, 0.85f, 0.76f, 1.0f);
    const ImVec4 muted = ImVec4(0.56f, 0.54f, 0.48f, 1.0f);
    const ImVec4 border = ImVec4(0.26f, 0.24f, 0.18f, 1.0f);

    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = panel;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_FrameBg] = input;
    c[ImGuiCol_FrameBgHovered] = hover;
    c[ImGuiCol_FrameBgActive] = hover;
    c[ImGuiCol_TitleBg] = panel;
    c[ImGuiCol_TitleBgActive] = header;
    c[ImGuiCol_Header] = header;
    c[ImGuiCol_HeaderHovered] = hover;
    c[ImGuiCol_HeaderActive] = hover;
    c[ImGuiCol_Button] = header;
    c[ImGuiCol_ButtonHovered] = hover;
    c[ImGuiCol_ButtonActive] = ImVec4(acc.x, acc.y, acc.z, 0.55f);
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_CheckMark] = acc;
    c[ImGuiCol_SliderGrab] = acc;
    c[ImGuiCol_SliderGrabActive] = acc;
    c[ImGuiCol_PlotHistogram] = acc;
    c[ImGuiCol_DockingPreview] = acc;
    c[ImGuiCol_ScrollbarBg] = input;
    c[ImGuiCol_TextSelectedBg] = ImVec4(acc.x, acc.y, acc.z, 0.30f);

    s.WindowRounding = 6.0f;
    s.ChildRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.PopupRounding = 6.0f;
    s.TabRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.WindowPadding = ImVec2(12, 12);
    s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 7);
    s.WindowBorderSize = 1.0f;
}

// A bordered dice-roll panel matching the original: the roll's label, large
// dice-face icons (animated while rolling), the per-die breakdown, the total,
// and the target.
void dice_panel(IconCache& icons, const oce::DiceRoll& roll, bool rolling, const ImVec4& accent) {
    static const char* const kFaces[] = {
        "dice-six-faces-one",  "dice-six-faces-two",  "dice-six-faces-three",
        "dice-six-faces-four", "dice-six-faces-five", "dice-six-faces-six"};
    ImGui::PushStyleColor(ImGuiCol_Border, accent);
    if (ImGui::BeginChild("##dicepanel", ImVec2(0.0f, 204.0f), ImGuiChildFlags_Borders)) {
        const ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        auto center = [](float item_w) {
            const float avail = ImGui::GetContentRegionAvail().x;
            const float base = ImGui::GetCursorPosX();
            if (item_w < avail) {
                ImGui::SetCursorPosX(base + (avail - item_w) * 0.5f);
            }
        };
        auto center_text = [&](const std::string& str, ImFont* font, const ImVec4& col) {
            if (font != nullptr) {
                ImGui::PushFont(font);
            }
            center(ImGui::CalcTextSize(str.c_str()).x);
            ImGui::TextColored(col, "%s", str.c_str());
            if (font != nullptr) {
                ImGui::PopFont();
            }
        };

        ImGui::Spacing();
        center_text(roll.name.empty() ? "Roll" : roll.name, g_heading_font, accent);
        ImGui::Spacing();

        const int count = (rolling || roll.dice.empty()) ? 2 : (int) roll.dice.size();
        const float die = 54.0f;
        const float spacing = 12.0f;
        center((float) count * die + (float) (count - 1) * spacing);
        for (int i = 0; i < count; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, spacing);
            }
            int face = 0;
            if (rolling) {
                const long t = (long) (ImGui::GetTime() * 12.0) + (long) i * 2;
                face = (int) (((t % 6) + 6) % 6);
            } else {
                const int v = roll.dice[(size_t) i];
                face = (v >= 1 && v <= 6) ? v - 1 : 0;
            }
            icons.draw(kFaces[face], die, rolling ? muted : ImVec4(0.93f, 0.82f, 0.42f, 1.0f));
        }
        ImGui::Spacing();

        if (rolling) {
            center_text("Rolling…", g_body_font, muted);
        } else {
            std::string rolls = "Rolls: ";
            for (size_t i = 0; i < roll.dice.size(); ++i) {
                if (i != 0) {
                    rolls += " + ";
                }
                rolls += std::to_string(roll.dice[i]);
            }
            if (roll.modifier != 0) {
                rolls += (roll.modifier > 0 ? "  +" : "  ") + std::to_string(roll.modifier);
            }
            center_text(rolls, g_body_font, muted);
            center_text("Total: " + std::to_string(roll.total), g_heading_font,
                        roll.success ? ImVec4(0.50f, 0.85f, 0.50f, 1.0f) : accent);
            center_text("Target: " + std::to_string(roll.target), g_body_font, muted);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
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
    gm_item_name_[0] = '\0';
    gm_item_desc_[0] = '\0';
    gm_mount_name_[0] = '\0';
    gm_mount_type_[0] = '\0';
    gm_mount_desc_[0] = '\0';
    gm_faction_[0] = '\0';
    gm_prop_name_[0] = '\0';
    gm_prop_location_[0] = '\0';
    gm_prop_desc_[0] = '\0';
    std::snprintf(new_name_, sizeof new_name_, "%s", "Adventurer");
    std::snprintf(camp_name_, sizeof camp_name_, "%s", "Adventure");
}

void GamePanels::setup_dock_layout(unsigned int dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dockspace_id;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.24f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Character", left);
    ImGui::DockBuilderDockWindow("Story", center);
    ImGui::DockBuilderFinish(dockspace_id);
}

void GamePanels::draw(oce::Engine& engine) {
    icons_.ensure_loaded();
    const oce::Snapshot s = engine.snapshot();
    apply_theme(s.theme);
    build_layout(engine, s);
    draw_modals(engine, s);
}

void GamePanels::build_layout(oce::Engine& engine, const oce::Snapshot& s) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    const ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##oce_host", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    // --- menu bar ---
    if (ImGui::BeginMenuBar()) {
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
            ImGui::Separator();
            if (ImGui::MenuItem("Settings...")) {
                show_settings_ = true;
                std::snprintf(model_buf_, sizeof model_buf_, "%s", s.model.c_str());
                std::snprintf(base_url_buf_, sizeof base_url_buf_, "%s", s.base_url.c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                reset_layout_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // --- top stat-bar ---
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 10));
    if (ImGui::BeginChild("##statbar", ImVec2(0, 78), ImGuiChildFlags_None)) {
        icons_.draw(class_icon(s.player.cls), 44.0f, accent_for(s.theme));
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::BeginGroup();
        heading(s.player.name.c_str());
        ImGui::TextDisabled("Level %d %s", s.player.level, class_to_string(s.player.cls));
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 32.0f);
        const float gap = 18.0f;
        const float slot_w = (ImGui::GetContentRegionAvail().x - 2.0f * gap) / 3.0f;
        const long long xp_next = oce::xp_for_next_level(s.player.level);
        const ImVec4 acc = accent_for(s.theme);
        const ImVec4 bar_border(acc.x, acc.y, acc.z, 0.70f);
        auto slot = [&](const char* icon, const char* label, long long cur, long long maxv,
                        const ImVec4& col) {
            ImGui::BeginGroup();
            icons_.draw(icon, 15.0f, col);
            ImGui::SameLine(0.0f, 5.0f);
            ImGui::TextColored(col, "%s", label);
            const std::string overlay = std::to_string(cur) + " / " + std::to_string(maxv);
            const float frac = maxv > 0 ? (float) cur / (float) maxv : 0.0f;
            gradient_bar(slot_w, 18.0f, frac, col, overlay.c_str(), bar_border);
            ImGui::EndGroup();
        };
        slot("health-normal", "HP", s.player.hp, s.player.max_hp, ImVec4(0.82f, 0.27f, 0.27f, 1.0f));
        ImGui::SameLine(0.0f, gap);
        slot("lightning-arc", "Energy", s.player.energy, s.player.max_energy,
             ImVec4(0.28f, 0.55f, 0.88f, 1.0f));
        ImGui::SameLine(0.0f, gap);
        slot("laurel-crown", "XP", s.player.xp, xp_next, accent_for(s.theme));
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // --- dockspace ---
    const ImGuiID dock_id = ImGui::GetID("oce_dockspace");
    if (reset_layout_) {
        setup_dock_layout(dock_id);
        reset_layout_ = false;
    }
    ImGui::DockSpace(dock_id);
    ImGui::End(); // host

    // --- Character panel (left dock) ---
    if (ImGui::Begin("Character")) {
        heading(s.player.name.c_str());
        ImGui::TextDisabled("Level %d %s · %s", s.player.level, class_to_string(s.player.cls),
                            oce::difficulty_to_string(s.meta.difficulty));
        ImGui::Spacing();

        ImGui::SeparatorText("Attributes");
        for (int i = 0; i < 4; ++i) { // primary four
            const AttrInfo& a = kAttrs[i];
            icons_.draw(a.icon, 18.0f, a.color);
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::Text("%-12s", a.label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 4.0f);
            ImGui::Text("%d", oce::attribute_value(s.player.attributes, a.key));
        }
        if (s.player.attribute_points > 0) {
            ImGui::TextColored(accent_for(s.theme), "%d point(s) to allocate", s.player.attribute_points);
        }
        if (ImGui::SmallButton("More attributes")) {
            show_stats_ = true;
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Inventory & Gold");
        icons_.draw("two-coins", 18.0f, ImVec4(0.90f, 0.78f, 0.32f, 1.0f));
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::Text("Gold: %d", s.player.gold);
        if (ImGui::Button("Show Inventory", ImVec2(-1.0f, 0.0f))) {
            show_inventory_ = true;
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Manage");
        if (ImGui::Button("Assets & Relations", ImVec2(-1.0f, 0.0f))) {
            show_assets_ = true;
        }
        if (ImGui::Button("Save / Load Game", ImVec2(-1.0f, 0.0f))) {
            show_load_ = true;
            saves_ = engine.list_saves();
        }
        if (ImGui::Button("Campaigns", ImVec2(-1.0f, 0.0f))) {
            show_characters_ = true;
            characters_ = engine.list_characters();
        }
        if (ImGui::Button("Game Master Tools", ImVec2(-1.0f, 0.0f))) {
            show_gm_ = true;
        }
        if (ImGui::Button("Settings", ImVec2(-1.0f, 0.0f))) {
            show_settings_ = true;
            std::snprintf(model_buf_, sizeof model_buf_, "%s", s.model.c_str());
            std::snprintf(base_url_buf_, sizeof base_url_buf_, "%s", s.base_url.c_str());
        }
        if (s.total_tokens > 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("Tokens: %lld", s.total_tokens);
        }
    }
    ImGui::End();

    // --- Story panel (center dock) ---
    if (ImGui::Begin("Story")) {
        heading(s.meta.name.empty() ? "Adventure" : s.meta.name.c_str());
        const float footer = ImGui::GetFrameHeightWithSpacing() * 3.4f;
        if (ImGui::BeginChild("log", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
            const ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            for (const oce::Message& m : s.story) {
                icons_.draw(sender_icon(m.sender), 16.0f,
                            m.sender == "player"   ? ImVec4(0.60f, 0.80f, 1.0f, 1.0f)
                            : m.sender == "system" ? ImVec4(0.85f, 0.45f, 0.40f, 1.0f)
                                                   : ImVec4(0.70f, 0.66f, 0.58f, 1.0f));
                ImGui::SameLine(0.0f, 6.0f);
                if (m.sender == "player") {
                    render_markdown(m.content, ImVec4(0.60f, 0.80f, 1.0f, 1.0f));
                } else if (m.sender == "system") {
                    render_markdown(m.content, ImVec4(0.82f, 0.55f, 0.50f, 1.0f));
                } else {
                    render_markdown(m.content, text_col);
                }
                ImGui::Spacing();
            }
            if (!s.streaming_text.empty()) {
                render_markdown(s.streaming_text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        if (!s.status.empty()) {
            ImGui::TextDisabled("%s", s.status.c_str());
        }

        const ImVec4 accent = accent_for(s.theme);
        const bool pending_roll = s.combat.active || s.skill_check.active;

        if (pending_roll) {
            // A roll is pending: gate further play behind an engage prompt that
            // appears once the narration has finished streaming.
            if (s.turn_in_progress) {
                ImGui::TextDisabled("…");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.30f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImVec4(accent.x, accent.y, accent.z, 0.48f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      ImVec4(accent.x, accent.y, accent.z, 0.62f));
                if (s.combat.active) {
                    icons_.draw("crossed-swords", 24.0f, accent);
                    ImGui::SameLine();
                    if (ImGui::Button("Combat!  —  engage", ImVec2(-1.0f, 38.0f))) {
                        combat_target_ = 0;
                        open_combat_ = true; // OpenPopup must run at the modal's scope
                    }
                }
                if (s.skill_check.active && !show_skill_) {
                    icons_.draw("dice-six-faces-five", 24.0f, accent);
                    ImGui::SameLine();
                    const std::string lbl =
                        "Skill Check: " + s.skill_check.attribute + "  —  roll";
                    if (ImGui::Button(lbl.c_str(), ImVec2(-1.0f, 38.0f))) {
                        show_skill_ = true;
                        skill_base_seq_ = s.last_roll.seq;
                    }
                }
                ImGui::PopStyleColor(3);
            }
        } else {
            // Quick-actions row: model suggestions, then always-present defaults.
            ImGui::BeginDisabled(s.turn_in_progress);
            const ImGuiStyle& style = ImGui::GetStyle();
            const float avail = ImGui::GetContentRegionAvail().x;
            float used = 0.0f;
            bool first = true;
            auto action_button = [&](const char* icon, const std::string& label, bool& clicked) {
                const float w =
                    ImGui::CalcTextSize(label.c_str()).x + 22.0f + style.FramePadding.x * 2.0f;
                if (!first && used + style.ItemSpacing.x + w <= avail) {
                    ImGui::SameLine();
                    used += style.ItemSpacing.x + w;
                } else {
                    used = w;
                }
                first = false;
                ImGui::PushID(label.c_str());
                clicked = false;
                if (icons_.has(icon)) {
                    icons_.draw(icon, 14.0f, ImGui::GetStyleColorVec4(ImGuiCol_Text));
                    ImGui::SameLine(0.0f, 4.0f);
                }
                clicked = ImGui::SmallButton(label.c_str());
                ImGui::PopID();
            };
            for (const std::string& a : s.suggested_actions) {
                bool clicked = false;
                action_button("run", a, clicked);
                if (clicked) {
                    engine.submit_turn(a);
                }
            }
            bool c1 = false, c2 = false, c3 = false;
            action_button("scroll-unfurled", "Continue", c1);
            if (c1) {
                engine.submit_turn("Continue");
            }
            action_button("magnifying-glass", "Look Around", c2);
            if (c2) {
                engine.submit_turn("Look around");
            }
            action_button("knapsack", "Check Inventory", c3);
            if (c3) {
                show_inventory_ = true;
            }
            ImGui::EndDisabled();
        }

        // The command box is locked while a turn resolves or a roll is pending.
        ImGui::BeginDisabled(s.turn_in_progress || pending_roll);
        ImGui::SetNextItemWidth(-90.0f);
        const bool enter = ImGui::InputTextWithHint(
            "##input", pending_roll ? "Resolve the prompt above to continue…" : "What do you do?",
            input_, sizeof input_, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool send = ImGui::Button("Send", ImVec2(-1.0f, 0.0f));
        ImGui::EndDisabled();
        if ((enter || send) && input_[0] != '\0' && !s.turn_in_progress && !pending_roll) {
            engine.submit_turn(input_);
            input_[0] = '\0';
        }
        if (s.turn_in_progress) {
            if (ImGui::Button("Cancel")) {
                engine.cancel_turn();
            }
        }
    }
    ImGui::End();
}

void GamePanels::draw_modals(oce::Engine& engine, const oce::Snapshot& s) {
    const oce::CharacterClass kClasses[] = {
        oce::CharacterClass::Warrior, oce::CharacterClass::Rogue, oce::CharacterClass::Mage,
        oce::CharacterClass::Cleric,  oce::CharacterClass::Ranger, oce::CharacterClass::Bard};
    const char* const kClassNames[] = {"Warrior", "Rogue", "Mage", "Cleric", "Ranger", "Bard"};
    const char* const kClassDesc[] = {
        "A mighty melee combatant. High strength and constitution.",
        "A cunning trickster. High dexterity and stealth.",
        "A master of arcane arts. High intelligence and wisdom.",
        "A divine healer and protector. High wisdom and charisma.",
        "A wilderness expert and marksman. High perception and dexterity.",
        "A charismatic jack-of-all-trades. High charisma and bartering."};

    // ---- New Game: Forge Your Destiny ----
    if (show_new_game_) {
        ImGui::OpenPopup("Forge Your Destiny");
        show_new_game_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Forge Your Destiny", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextDisabled("Create your character and customize your world setting.");
        ImGui::Spacing();
        ImGui::InputText("Name", new_name_, sizeof new_name_);
        if (ImGui::BeginTabBar("new_game_tabs")) {
            if (ImGui::BeginTabItem("Class")) {
                for (int i = 0; i < 6; ++i) {
                    if (i % 3 != 0) {
                        ImGui::SameLine();
                    }
                    const bool sel = (new_class_ == i);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    }
                    ImGui::PushID(i);
                    if (ImGui::Button(kClassNames[i], ImVec2(150, 0))) {
                        new_class_ = i;
                    }
                    ImGui::PopID();
                    if (sel) {
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::Spacing();
                const int idx = (new_class_ >= 0 && new_class_ < 6) ? new_class_ : 0;
                icons_.draw(class_icon(kClasses[idx]), 40.0f, accent_for(s.theme));
                ImGui::SameLine();
                ImGui::BeginGroup();
                heading(kClassNames[idx]);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextDisabled("%s", kClassDesc[idx]);
                ImGui::PopTextWrapPos();
                ImGui::EndGroup();
                ImGui::Spacing();
                ImGui::SeparatorText("Starting Attributes");
                const oce::Attributes a = oce::starting_attributes(kClasses[idx]);
                for (int i = 0; i < 10; ++i) {
                    if (i % 2 != 0) {
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f + 90.0f);
                    }
                    icons_.draw(kAttrs[i].icon, 16.0f, kAttrs[i].color);
                    ImGui::SameLine(0.0f, 5.0f);
                    ImGui::Text("%-13s %d", kAttrs[i].label, oce::attribute_value(a, kAttrs[i].key));
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("World")) {
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
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Premise")) {
                ImGui::InputTextMultiline("##premise", new_world_, sizeof new_world_,
                                          ImVec2(-1.0f, 120.0f));
                if (ImGui::Button("Suggest premise")) {
                    awaiting_autofill_ = true;
                    last_autofill_seq_ = s.autofill_seq;
                    oce::WorldParams wp;
                    wp.biome = oce::BIOME_OPTIONS[(size_t) wp_biome_];
                    engine.request_autofill(wp, "premise");
                }
                if (s.turn_in_progress) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("Working…");
                }
                if (awaiting_autofill_ && s.autofill_seq > last_autofill_seq_) {
                    std::snprintf(new_world_, sizeof new_world_, "%s", s.autofill_value.c_str());
                    awaiting_autofill_ = false;
                }
                ImGui::InputTextMultiline("Background", new_background_, sizeof new_background_,
                                          ImVec2(-1.0f, 80.0f));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        auto pick = [](const auto& arr, int i) -> std::string {
            return (i >= 0 && (size_t) i < arr.size()) ? std::string(arr[(size_t) i]) : std::string();
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
        oce::NewGameParams p;
        p.name = (new_name_[0] != '\0') ? new_name_ : "Adventurer";
        p.cls = kClasses[(new_class_ >= 0 && new_class_ < 6) ? new_class_ : 0];
        p.background = new_background_;
        p.world_prompt = new_world_;
        ImGui::BeginDisabled(s.turn_in_progress);
        if (ImGui::Button("Begin Adventure", ImVec2(160, 0))) {
            engine.new_game(p);
            engine.generate_world(make_params());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Quick Start")) {
            engine.new_game(p);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Inventory ----
    if (show_inventory_) {
        ImGui::SetNextWindowSize(ImVec2(440, 460), ImGuiCond_Appearing);
        if (ImGui::Begin("Inventory & Equipment", &show_inventory_)) {
            ImGui::TextDisabled("Equipped");
            auto slot = [&](const char* label, const std::optional<oce::Item>& item,
                            const char* slot_key) {
                ImGui::BeginGroup();
                ImGui::TextDisabled("%s", label);
                if (item.has_value()) {
                    const char* ic = (!item->icon.empty() && icons_.has(item->icon))
                                         ? item->icon.c_str()
                                         : kind_icon(item->kind);
                    icons_.draw(ic, 40.0f, rarity_color(item->rarity));
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImGui::TextColored(rarity_color(item->rarity), "%s", item->name.c_str());
                    if (ImGui::SmallButton((std::string("Unequip##") + slot_key).c_str())) {
                        engine.player_unequip(slot_key);
                    }
                    ImGui::EndGroup();
                } else {
                    ImGui::TextDisabled("(empty)");
                }
                ImGui::EndGroup();
            };
            slot("Hand", s.equipment.hand, "hand");
            slot("Body", s.equipment.body, "body");

            ImGui::Separator();
            ImGui::TextDisabled("Backpack (%zu)", s.inventory.size());
            if (ImGui::BeginChild("backpack", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
                const float avail = ImGui::GetContentRegionAvail().x;
                const float tile = 52.0f;
                int per_row = (int) (avail / (tile + ImGui::GetStyle().ItemSpacing.x));
                if (per_row < 1) {
                    per_row = 1;
                }
                int col = 0;
                for (size_t i = 0; i < s.inventory.size(); ++i) {
                    const oce::Item& it = s.inventory[i];
                    if (col != 0) {
                        ImGui::SameLine();
                    }
                    const char* ic = (!it.icon.empty() && icons_.has(it.icon)) ? it.icon.c_str()
                                                                              : kind_icon(it.kind);
                    ImGui::PushID((int) i);
                    if (icons_.button(ic, tile, rarity_color(it.rarity))) {
                        if (it.kind == oce::ItemKind::Potion) {
                            engine.player_consume(it.id);
                        } else {
                            engine.player_equip(it.id);
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(rarity_color(it.rarity), "%s", it.name.c_str());
                        ImGui::TextDisabled("%s %s", rarity_label(it.rarity), kind_label(it.kind));
                        if (!it.description.empty()) {
                            ImGui::TextWrapped("%s", it.description.c_str());
                        }
                        const std::string eff = effects_summary(it.effects);
                        if (!eff.empty()) {
                            ImGui::TextUnformatted(eff.c_str());
                        }
                        ImGui::TextDisabled(it.kind == oce::ItemKind::Potion ? "(click to use)"
                                                                            : "(click to equip)");
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();
                    col = (col + 1) % per_row;
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    // ---- Character Statistics ----
    if (show_stats_) {
        ImGui::SetNextWindowSize(ImVec2(360, 460), ImGuiCond_Appearing);
        if (ImGui::Begin("Character Statistics", &show_stats_)) {
            if (s.player.attribute_points > 0) {
                ImGui::TextColored(accent_for(s.theme), "%d point(s) available",
                                   s.player.attribute_points);
            } else {
                ImGui::TextDisabled("No points to allocate");
            }
            ImGui::Separator();
            const bool can = s.player.attribute_points > 0 && !s.turn_in_progress;
            for (int i = 0; i < 10; ++i) {
                const AttrInfo& a = kAttrs[i];
                icons_.draw(a.icon, 20.0f, a.color);
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::Text("%-13s", a.label);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50.0f);
                ImGui::Text("%2d", oce::attribute_value(s.player.attributes, a.key));
                if (can) {
                    ImGui::SameLine();
                    ImGui::PushID(a.key);
                    if (ImGui::SmallButton("+")) {
                        engine.allocate_attribute(a.key);
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::End();
    }

    // ---- Assets & Relations ----
    if (show_assets_) {
        const ImVec4 acc = accent_for(s.theme);
        int biz_value = 0;
        int prop_value = 0;
        int daily = 0;
        int pending = 0;
        for (const oce::Business& b : s.assets.businesses) {
            biz_value += b.value;
            daily += b.income_per_day;
            pending += oce::pending_business_income(b, s.world_state.time_elapsed);
        }
        for (const oce::Property& p : s.assets.properties) {
            prop_value += p.value;
        }
        ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Appearing);
        if (ImGui::Begin("Assets & Relations", &show_assets_)) {
            ImGui::TextDisabled("Manage your businesses, properties, mounts, and key "
                                "relationships.");
            auto summary = [&](const char* label, const std::string& value, const char* icon) {
                ImGui::BeginChild(label, ImVec2(ImGui::GetContentRegionAvail().x / 3.0f - 6.0f, 60.0f),
                                  ImGuiChildFlags_Borders);
                icons_.draw(icon, 16.0f, acc);
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::TextDisabled("%s", label);
                if (g_heading_font != nullptr) {
                    ImGui::PushFont(g_heading_font);
                }
                ImGui::TextColored(acc, "%s", value.c_str());
                if (g_heading_font != nullptr) {
                    ImGui::PopFont();
                }
                ImGui::EndChild();
            };
            summary("Business Value", std::to_string(biz_value) + " gold", "briefcase");
            ImGui::SameLine();
            summary("Property Value", std::to_string(prop_value) + " gold", "castle");
            ImGui::SameLine();
            summary("Daily Income", "+" + std::to_string(daily) + "/day", "two-coins");
            ImGui::Spacing();

            const auto card_begin = [](const char* id) {
                ImGui::BeginChild(id, ImVec2(0.0f, 0.0f),
                                  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            };
            const auto wrap_desc = [](const std::string& d) {
                if (!d.empty()) {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("%s", d.c_str());
                    ImGui::PopTextWrapPos();
                }
            };

            if (ImGui::BeginTabBar("assets_tabs")) {
                if (ImGui::BeginTabItem(
                        ("Businesses (" + std::to_string(s.assets.businesses.size()) + ")###biz")
                            .c_str())) {
                    ImGui::BeginDisabled(s.turn_in_progress || pending <= 0);
                    if (ImGui::Button("Collect income")) {
                        engine.collect_income();
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::TextDisabled("accrued: %d gold", pending);
                    for (const oce::Business& b : s.assets.businesses) {
                        ImGui::PushID(b.id.c_str());
                        card_begin("c");
                        icons_.draw(business_type_icon(b.type), 22.0f, acc);
                        ImGui::SameLine();
                        ImGui::TextColored(acc, "%s", b.name.c_str());
                        ImGui::SameLine();
                        badge(business_type_label(b.type), acc);
                        wrap_desc(b.description);
                        ImGui::Text("Location: %s", b.location.empty() ? "—" : b.location.c_str());
                        ImGui::Text("Value: %d gold     Income: +%d/day", b.value, b.income_per_day);
                        ImGui::EndChild();
                        ImGui::PopID();
                    }
                    if (s.assets.businesses.empty()) {
                        ImGui::TextDisabled("No businesses yet.");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(
                        ("Properties (" + std::to_string(s.assets.properties.size()) + ")###prop")
                            .c_str())) {
                    for (const oce::Property& p : s.assets.properties) {
                        ImGui::PushID(p.id.c_str());
                        card_begin("c");
                        icons_.draw("castle", 22.0f, acc);
                        ImGui::SameLine();
                        ImGui::TextColored(acc, "%s", p.name.c_str());
                        ImGui::SameLine();
                        badge(p.type.empty() ? "property" : p.type.c_str(), acc);
                        wrap_desc(p.description);
                        ImGui::Text("Location: %s", p.location.empty() ? "—" : p.location.c_str());
                        ImGui::Text("Value: %d gold", p.value);
                        ImGui::EndChild();
                        ImGui::PopID();
                    }
                    if (s.assets.properties.empty()) {
                        ImGui::TextDisabled("No properties yet.");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(
                        ("Mounts (" + std::to_string(s.assets.mounts.size()) + ")###mnt").c_str())) {
                    ImGui::BeginDisabled(s.turn_in_progress);
                    if (ImGui::Button("Acquire a mount")) {
                        engine.acquire_mount();
                    }
                    ImGui::EndDisabled();
                    for (const oce::MountVehicle& m : s.assets.mounts) {
                        ImGui::PushID(m.id.c_str());
                        card_begin("c");
                        icons_.draw("horse-head", 22.0f, acc);
                        ImGui::SameLine();
                        ImGui::TextColored(acc, "%s", m.name.c_str());
                        ImGui::SameLine();
                        badge(m.type.empty() ? "mount" : m.type.c_str(), acc);
                        wrap_desc(m.description);
                        ImGui::Text("Speed x%.1f     Capacity %d     Upkeep %d/day", m.speed,
                                    m.capacity, m.upkeep_cost);
                        ImGui::Text("Condition");
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, acc);
                        ImGui::ProgressBar((float) m.condition / 100.0f, ImVec2(-1.0f, 10.0f), "");
                        ImGui::PopStyleColor();
                        ImGui::EndChild();
                        ImGui::PopID();
                    }
                    if (s.assets.mounts.empty()) {
                        ImGui::TextDisabled("No mounts yet.");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(
                        ("Relations (" + std::to_string(s.assets.relations.size()) + ")###rel")
                            .c_str())) {
                    for (const oce::Relation& r : s.assets.relations) {
                        ImGui::PushID(r.id.c_str());
                        card_begin("c");
                        icons_.draw(r.strength >= 0 ? "hearts" : "sword-clash", 22.0f, acc);
                        ImGui::SameLine();
                        ImGui::TextColored(acc, "%s", r.npc_name.c_str());
                        ImGui::SameLine();
                        badge(r.type.empty() ? "acquaintance" : r.type.c_str(), acc);
                        wrap_desc(r.description);
                        standing_bar(r.strength);
                        if (r.strength >= 25) {
                            for (const std::string& ben : r.benefits) {
                                ImGui::TextDisabled("• %s", ben.c_str());
                            }
                        } else if (!r.benefits.empty()) {
                            ImGui::TextDisabled("(benefits unlock as the bond strengthens)");
                        }
                        ImGui::EndChild();
                        ImGui::PopID();
                    }
                    if (s.assets.relations.empty()) {
                        ImGui::TextDisabled("No relationships yet.");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(
                        ("Factions (" + std::to_string(s.world_state.factions.size()) + ")###fac")
                            .c_str())) {
                    for (const auto& kv : s.world_state.factions) {
                        const oce::Faction& f = kv.second;
                        ImGui::PushID(f.id.c_str());
                        card_begin("c");
                        icons_.draw(faction_type_icon(f.type), 22.0f, acc);
                        ImGui::SameLine();
                        ImGui::TextColored(acc, "%s", f.name.c_str());
                        ImGui::SameLine();
                        badge(faction_type_label(f.type), acc);
                        wrap_desc(f.description);
                        standing_bar(f.relationship);
                        ImGui::Text("Standing: %d     Reputation: %d/1000", f.relationship,
                                    f.reputation);
                        if (!f.territory.empty()) {
                            ImGui::Text("Territory: %s", f.territory.c_str());
                        }
                        if (!f.leader.empty()) {
                            ImGui::Text("Leader: %s", f.leader.c_str());
                        }
                        if (f.relationship >= 25) {
                            for (const std::string& ben : f.benefits) {
                                ImGui::TextDisabled("• %s", ben.c_str());
                            }
                        } else if (!f.benefits.empty()) {
                            ImGui::TextDisabled("(benefits unlock at standing 25+)");
                        }
                        ImGui::EndChild();
                        ImGui::PopID();
                    }
                    if (s.world_state.factions.empty()) {
                        ImGui::TextDisabled("No factions discovered yet.");
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    // ---- Characters ----
    if (show_characters_) {
        ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_Appearing);
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
                ImGui::TextDisabled("No characters yet.");
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
                    break;
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    // ---- Campaign Manager ----
    if (show_campaigns_) {
        ImGui::SetNextWindowSize(ImVec2(460, 460), ImGuiCond_Appearing);
        if (ImGui::Begin("Campaign Manager", &show_campaigns_)) {
            if (ImGui::Button("Refresh")) {
                campaigns_ = engine.list_campaigns(selected_char_);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Campaigns");
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
                    break;
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
            ImGui::InputText("Goals (comma)##camp", camp_goals_, sizeof camp_goals_);
            const char* diffs[] = {"Easy", "Normal", "Hard", "Deadly"};
            ImGui::Combo("Difficulty##camp", &camp_difficulty_, diffs, 4);
            ImGui::InputTextMultiline("Custom prompt##camp", camp_custom_, sizeof camp_custom_,
                                      ImVec2(0.0f, 50.0f));
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

    // ---- Load Game ----
    if (show_load_) {
        ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_Appearing);
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

    // ---- Settings ----
    if (show_settings_) {
        ImGui::SetNextWindowSize(ImVec2(440, 320), ImGuiCond_Appearing);
        if (ImGui::Begin("Settings", &show_settings_)) {
            ImGui::TextWrapped("Your OpenRouter API key is held in memory only and never written "
                               "to disk.");
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
        }
        ImGui::End();
    }

    // ---- Game Master Tools ----
    if (show_gm_) {
        const ImVec4 gm_accent = accent_for(s.theme);
        const char* types[] = {"weapon", "armor", "potion"};
        const char* rars[] = {"common", "uncommon", "rare", "epic", "legendary"};
        const char* slots[] = {"hand", "body", "consumable"};
        const char* ptypes[] = {"house", "estate", "castle", "hideout", "tower", "ship", "other"};
        const std::vector<std::string>& icon_names = icons_.names();
        ImGui::SetNextWindowSize(ImVec2(660, 600), ImGuiCond_Appearing);
        if (ImGui::Begin("Game Master Tools", &show_gm_)) {
            ImGui::TextDisabled("Manually add, edit, or remove game elements. "
                                "Changes take effect immediately.");
            if (ImGui::BeginTabBar("gm_tabs")) {
                if (ImGui::BeginTabItem("Items")) {
                    ImGui::SeparatorText("Add New Item");
                    ImGui::InputText("Name##gmi", gm_item_name_, sizeof gm_item_name_);
                    if (gm_item_icon_ >= (int) icon_names.size()) {
                        gm_item_icon_ = 0;
                    }
                    if (!icon_names.empty()) {
                        icons_.draw(icon_names[(size_t) gm_item_icon_], 22.0f, gm_accent);
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("Icon##gmi", icon_names[(size_t) gm_item_icon_].c_str())) {
                            for (int i = 0; i < (int) icon_names.size(); ++i) {
                                ImGui::PushID(i);
                                icons_.draw(icon_names[(size_t) i], 18.0f, gm_accent);
                                ImGui::SameLine();
                                if (ImGui::Selectable(icon_names[(size_t) i].c_str(),
                                                      gm_item_icon_ == i)) {
                                    gm_item_icon_ = i;
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndCombo();
                        }
                    }
                    ImGui::Combo("Type##gmi", &gm_item_type_, types, 3);
                    ImGui::Combo("Rarity##gmi", &gm_item_rarity_, rars, 5);
                    ImGui::Combo("Slot##gmi", &gm_item_slot_, slots, 3);
                    ImGui::SliderInt("Power##gmi", &gm_item_power_, 1, 20);
                    ImGui::InputTextMultiline("Description##gmi", gm_item_desc_, sizeof gm_item_desc_,
                                              ImVec2(-1.0f, 48.0f));
                    if (ImGui::Button("Add Item", ImVec2(150, 0)) && gm_item_name_[0] != '\0') {
                        char json[640];
                        std::snprintf(json, sizeof json,
                                      "{\"name\":\"%s\",\"type\":\"%s\",\"rarity\":\"%s\",\"slot\":"
                                      "\"%s\",\"power\":%d,\"description\":\"%s\",\"icon\":\"%s\"}",
                                      gm_item_name_, types[gm_item_type_], rars[gm_item_rarity_],
                                      slots[gm_item_slot_], gm_item_power_, gm_item_desc_,
                                      icon_names.empty() ? ""
                                                         : icon_names[(size_t) gm_item_icon_].c_str());
                        engine.apply_gm_tool("add_item", json);
                        gm_item_name_[0] = '\0';
                        gm_item_desc_[0] = '\0';
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add Random")) {
                        engine.apply_gm_tool("add_random_item", "{}");
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText(
                        ("Current Items (" + std::to_string(s.inventory.size()) + ")").c_str());
                    for (size_t i = 0; i < s.inventory.size(); ++i) {
                        const oce::Item& it = s.inventory[i];
                        ImGui::PushID((int) i);
                        const char* ic = (!it.icon.empty() && icons_.has(it.icon))
                                             ? it.icon.c_str()
                                             : kind_icon(it.kind);
                        icons_.draw(ic, 18.0f, rarity_color(it.rarity));
                        ImGui::SameLine();
                        ImGui::TextColored(rarity_color(it.rarity), "%s", it.name.c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s %s)", rarity_label(it.rarity), kind_label(it.kind));
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
                        if (ImGui::SmallButton("x")) {
                            char json[96];
                            std::snprintf(json, sizeof json, "{\"item_id\":\"%s\"}", it.id.c_str());
                            engine.apply_gm_tool("remove_item", json);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Mounts")) {
                    ImGui::SeparatorText("Add New Mount");
                    ImGui::InputText("Name##gmm", gm_mount_name_, sizeof gm_mount_name_);
                    ImGui::InputText("Type##gmm", gm_mount_type_, sizeof gm_mount_type_);
                    ImGui::InputTextMultiline("Description##gmm", gm_mount_desc_,
                                              sizeof gm_mount_desc_, ImVec2(-1.0f, 40.0f));
                    ImGui::SliderInt("Speed x10##gmm", &gm_mount_speed_, 5, 50);
                    ImGui::SliderInt("Capacity##gmm", &gm_mount_capacity_, 0, 100);
                    ImGui::SliderInt("Upkeep##gmm", &gm_mount_upkeep_, 0, 50);
                    if (ImGui::Button("Add Mount", ImVec2(150, 0)) && gm_mount_name_[0] != '\0') {
                        char json[640];
                        std::snprintf(json, sizeof json,
                                      "{\"name\":\"%s\",\"type\":\"%s\",\"description\":\"%s\","
                                      "\"speed\":%.1f,\"capacity\":%d,\"upkeep_cost\":%d}",
                                      gm_mount_name_, gm_mount_type_[0] ? gm_mount_type_ : "horse",
                                      gm_mount_desc_, (double) gm_mount_speed_ / 10.0,
                                      gm_mount_capacity_, gm_mount_upkeep_);
                        engine.apply_gm_tool("add_mount", json);
                        gm_mount_name_[0] = '\0';
                        gm_mount_desc_[0] = '\0';
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Acquire (roster)")) {
                        engine.acquire_mount();
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText(
                        ("Current Mounts (" + std::to_string(s.assets.mounts.size()) + ")").c_str());
                    for (size_t i = 0; i < s.assets.mounts.size(); ++i) {
                        const oce::MountVehicle& m = s.assets.mounts[i];
                        ImGui::PushID((int) i);
                        icons_.draw("horse-head", 18.0f, gm_accent);
                        ImGui::SameLine();
                        ImGui::Text("%s (%s) — condition %d", m.name.c_str(), m.type.c_str(),
                                    m.condition);
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
                        if (ImGui::SmallButton("x")) {
                            char json[96];
                            std::snprintf(json, sizeof json, "{\"mount_id\":\"%s\"}", m.id.c_str());
                            engine.apply_gm_tool("remove_mount", json);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Factions")) {
                    ImGui::SeparatorText("Add / Adjust Faction");
                    ImGui::InputText("Name##gmf", gm_faction_, sizeof gm_faction_);
                    ImGui::SliderInt("Relationship##gmf", &gm_faction_rel_, -100, 100);
                    ImGui::SliderInt("Reputation##gmf", &gm_faction_rep_, 0, 1000);
                    if (ImGui::Button("Apply Faction", ImVec2(150, 0)) && gm_faction_[0] != '\0') {
                        std::string id = gm_faction_;
                        for (char& ch : id) {
                            ch = (ch == ' ') ? '-' : (char) std::tolower((unsigned char) ch);
                        }
                        char json[256];
                        std::snprintf(json, sizeof json,
                                      "{\"faction_id\":\"%s\",\"name\":\"%s\",\"relationship_"
                                      "change\":%d,\"reputation_change\":%d}",
                                      id.c_str(), gm_faction_, gm_faction_rel_, gm_faction_rep_);
                        engine.apply_gm_tool("change_faction", json);
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText(
                        ("Current Factions (" + std::to_string(s.world_state.factions.size()) + ")")
                            .c_str());
                    for (const auto& kv : s.world_state.factions) {
                        const oce::Faction& f = kv.second;
                        icons_.draw("crossed-swords", 18.0f, gm_accent);
                        ImGui::SameLine();
                        ImGui::Text("%s — standing %d, reputation %d", f.name.c_str(),
                                    f.relationship, f.reputation);
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Properties")) {
                    ImGui::SeparatorText("Add New Property");
                    ImGui::InputText("Name##gmp", gm_prop_name_, sizeof gm_prop_name_);
                    ImGui::Combo("Type##gmp", &gm_prop_type_, ptypes, 7);
                    ImGui::InputText("Location##gmp", gm_prop_location_, sizeof gm_prop_location_);
                    ImGui::SliderInt("Value##gmp", &gm_prop_value_, 0, 100000);
                    ImGui::InputTextMultiline("Description##gmp", gm_prop_desc_, sizeof gm_prop_desc_,
                                              ImVec2(-1.0f, 40.0f));
                    if (ImGui::Button("Add Property", ImVec2(150, 0)) && gm_prop_name_[0] != '\0') {
                        char json[640];
                        std::snprintf(json, sizeof json,
                                      "{\"name\":\"%s\",\"type\":\"%s\",\"location\":\"%s\","
                                      "\"value\":%d,\"description\":\"%s\"}",
                                      gm_prop_name_, ptypes[gm_prop_type_], gm_prop_location_,
                                      gm_prop_value_, gm_prop_desc_);
                        engine.apply_gm_tool("add_property", json);
                        gm_prop_name_[0] = '\0';
                        gm_prop_location_[0] = '\0';
                        gm_prop_desc_[0] = '\0';
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText(
                        ("Current Properties (" + std::to_string(s.assets.properties.size()) + ")")
                            .c_str());
                    for (size_t i = 0; i < s.assets.properties.size(); ++i) {
                        const oce::Property& p = s.assets.properties[i];
                        ImGui::PushID((int) i);
                        icons_.draw("castle", 18.0f, gm_accent);
                        ImGui::SameLine();
                        ImGui::Text("%s (%s) — %d gold", p.name.c_str(), p.type.c_str(), p.value);
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
                        if (ImGui::SmallButton("x")) {
                            char json[96];
                            std::snprintf(json, sizeof json, "{\"property_id\":\"%s\"}",
                                          p.id.c_str());
                            engine.apply_gm_tool("remove_property", json);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Stats")) {
                    ImGui::SeparatorText("Grant / Adjust");
                    ImGui::SliderInt("Gold +/-##gms", &gm_gold_, -500, 500);
                    ImGui::SliderInt("XP +##gms", &gm_xp_, 0, 1000);
                    ImGui::SliderInt("HP +/-##gms", &gm_hp_, -50, 50);
                    ImGui::SliderInt("Energy +/-##gms", &gm_energy_, -50, 50);
                    if (ImGui::Button("Apply Stats", ImVec2(150, 0))) {
                        char json[160];
                        std::snprintf(json, sizeof json,
                                      "{\"gold\":%d,\"xp\":%d,\"hp\":%d,\"energy\":%d}", gm_gold_,
                                      gm_xp_, gm_hp_, gm_energy_);
                        engine.apply_gm_tool("apply_stat_changes", json);
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText("Attributes");
                    for (int i = 0; i < 10; ++i) {
                        const AttrInfo& a = kAttrs[i];
                        icons_.draw(a.icon, 16.0f, a.color);
                        ImGui::SameLine(0.0f, 5.0f);
                        ImGui::Text("%-13s %d", a.label,
                                    oce::attribute_value(s.player.attributes, a.key));
                        if (s.player.attribute_points > 0) {
                            ImGui::SameLine();
                            ImGui::PushID(a.key);
                            if (ImGui::SmallButton("+")) {
                                engine.allocate_attribute(a.key);
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::TextDisabled("Unspent points: %d", s.player.attribute_points);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    // ---- Combat (opened by the "Combat!" engage button above the input) ----
    // OpenPopup must be called in the same ID scope as BeginPopupModal, so the
    // engage button (in the Story window) only sets a flag we act on here.
    if (open_combat_) {
        ImGui::OpenPopup("Combat");
        open_combat_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(560, 660), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Combat", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (!s.combat.active) {
            ImGui::CloseCurrentPopup();
        } else {
            const oce::Item* weapon =
                s.equipment.hand.has_value() ? &s.equipment.hand.value() : nullptr;
            const oce::Item* armor =
                s.equipment.body.has_value() ? &s.equipment.body.value() : nullptr;
            ImGui::Text("Attack +%d    Defense %d", oce::player_attack_bonus(s.player, weapon),
                        oce::player_defense(s.player, armor));
            ImGui::Text("HP %d/%d", s.player.hp, s.player.max_hp);
            ImGui::Separator();
            ImGui::TextDisabled("Enemies (%zu remaining)", s.combat.enemies.size());
            if (combat_target_ >= (int) s.combat.enemies.size()) {
                combat_target_ = 0;
            }
            for (size_t i = 0; i < s.combat.enemies.size(); ++i) {
                const oce::Enemy& e = s.combat.enemies[i];
                ImGui::PushID((int) i);
                if (ImGui::RadioButton("##t", combat_target_ == (int) i)) {
                    combat_target_ = (int) i;
                }
                ImGui::SameLine();
                ImGui::Text("%s", e.name.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90.0f);
                const float frac = e.max_hp > 0 ? (float) e.hp / (float) e.max_hp : 0.0f;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.78f, 0.28f, 0.28f, 1.0f));
                ImGui::ProgressBar(frac, ImVec2(86.0f, 12.0f),
                                   (std::to_string(e.hp) + "/" + std::to_string(e.max_hp)).c_str());
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::Spacing();
            dice_panel(icons_, s.last_roll, s.turn_in_progress, accent_for(s.theme));
            ImGui::Spacing();
            ImGui::BeginDisabled(s.turn_in_progress);
            if (ImGui::Button("Attack")) {
                engine.combat_action("attack", combat_target_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Defend")) {
                engine.combat_action("defend", 0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Flee")) {
                engine.combat_action("flee", 0);
            }
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
            ImGui::TextDisabled("Combat Log");
            if (ImGui::BeginChild("combat_log", ImVec2(0, 120.0f), ImGuiChildFlags_Borders)) {
                for (const std::string& line : s.combat.log) {
                    ImGui::TextWrapped("%s", line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndPopup();
    }

    // ---- Skill Check (opened by the "Skill Check" engage button) ----
    if (show_skill_) {
        ImGui::SetNextWindowSize(ImVec2(440, 440), ImGuiCond_Appearing);
        if (ImGui::Begin("Skill Check", &show_skill_)) {
            const bool rolled = s.last_roll.seq > skill_base_seq_;
            if (s.skill_check.active) {
                heading(("Skill Check: " + s.skill_check.attribute).c_str());
                if (!s.skill_check.description.empty()) {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("%s", s.skill_check.description.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::Text("%s vs difficulty %d  (%dd6)", s.skill_check.attribute.c_str(),
                            s.skill_check.difficulty, s.skill_check.num_dice);
            } else {
                heading("Skill Check");
            }
            ImGui::Spacing();
            if (rolled) {
                dice_panel(icons_, s.last_roll, false, accent_for(s.theme));
                ImGui::Spacing();
                if (ImGui::Button("Continue", ImVec2(140, 0))) {
                    show_skill_ = false;
                }
            } else {
                ImGui::BeginDisabled(!s.skill_check.active || s.turn_in_progress);
                if (ImGui::Button("Roll the dice", ImVec2(160, 0))) {
                    engine.resolve_skill_check();
                }
                ImGui::EndDisabled();
            }
        }
        ImGui::End();
    }
}

} // namespace oce::ui
