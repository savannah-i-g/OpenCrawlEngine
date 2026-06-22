#pragma once
// IconCache — rasterizes the bundled game-icons.net SVGs into OpenGL textures
// and draws them through ImGui. The source icons are white on transparent, so
// each draw tints them to the caller's colour. Icons are looked up by their
// game-icons stem (e.g. "broadsword").
//
// Ownership : owns the GL textures it creates; frees them on destruction.
// Threading : UI-thread only. ensure_loaded() needs a current GL context, so it
//             is called lazily on the first frame rather than at construction.

#include "imgui.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace oce::ui {

class IconCache {
public:
    IconCache() = default;
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    // Loads and rasterizes every bundled icon once; a no-op thereafter.
    void ensure_loaded();

    bool has(const std::string& name) const;

    // Draws the icon inline at a square size, tinted (white = unchanged). A
    // missing icon reserves the same space so layouts stay stable.
    void draw(const std::string& name, float size, const ImVec4& tint = ImVec4(1, 1, 1, 1));

    // An icon-only button; returns true when clicked. Falls back to a labelled
    // button when the icon is missing.
    bool button(const std::string& name, float size, const ImVec4& tint = ImVec4(1, 1, 1, 1));

    // Sorted list of loaded icon names (for pickers). Empty until ensure_loaded.
    const std::vector<std::string>& names() const { return names_; }

private:
    bool loaded_ = false;
    std::unordered_map<std::string, ImTextureID> textures_;
    std::vector<std::string> names_;
    void load_one(const std::string& name, const std::string& path, void* rasterizer);
};

} // namespace oce::ui
