#include "oce_ui/icon_cache.hpp"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#include <GL/gl.h>

#include <cstdio>
#include <filesystem>
#include <vector>

#ifndef OCE_ASSET_DIR
#define OCE_ASSET_DIR "."
#endif

namespace oce::ui {

namespace {
constexpr int kRasterSize = 128; // each icon rasterized to 128x128 RGBA
} // namespace

IconCache::~IconCache() {
    for (const auto& kv : textures_) {
        GLuint tex = (GLuint) kv.second;
        if (tex != 0) {
            glDeleteTextures(1, &tex); // best-effort; harmless if the context is gone
        }
    }
}

void IconCache::load_one(const std::string& name, const std::string& path, void* rasterizer) {
    NSVGimage* image = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (image == nullptr) {
        return;
    }
    const float dim = image->width > 0.0f ? image->width : (float) kRasterSize;
    const float scale = (float) kRasterSize / dim;
    std::vector<unsigned char> rgba((size_t) kRasterSize * kRasterSize * 4, 0);
    nsvgRasterize((NSVGrasterizer*) rasterizer, image, 0.0f, 0.0f, scale, rgba.data(), kRasterSize,
                  kRasterSize, kRasterSize * 4);
    nsvgDelete(image);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kRasterSize, kRasterSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba.data());
    textures_[name] = (ImTextureID) tex;
}

void IconCache::ensure_loaded() {
    if (loaded_) {
        return;
    }
    loaded_ = true; // mark first so a failed scan does not retry every frame

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (rast == nullptr) {
        return;
    }
    const std::filesystem::path dir = std::filesystem::path(OCE_ASSET_DIR) / "icons";
    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.path().extension() == ".svg") {
                load_one(entry.path().stem().string(), entry.path().string(), rast);
            }
        }
    }
    nsvgDeleteRasterizer(rast);
    std::fprintf(stderr, "IconCache: loaded %zu icons from %s\n", textures_.size(),
                 (std::filesystem::path(OCE_ASSET_DIR) / "icons").c_str());
}

bool IconCache::has(const std::string& name) const {
    return textures_.find(name) != textures_.end();
}

void IconCache::draw(const std::string& name, float size, const ImVec4& tint) {
    const auto it = textures_.find(name);
    if (it == textures_.end()) {
        ImGui::Dummy(ImVec2(size, size));
        return;
    }
    ImGui::ImageWithBg(it->second, ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1),
                       ImVec4(0, 0, 0, 0), tint);
}

bool IconCache::button(const std::string& name, float size, const ImVec4& tint) {
    const auto it = textures_.find(name);
    if (it == textures_.end()) {
        return ImGui::Button(name.c_str(), ImVec2(size, size));
    }
    ImGui::PushID(name.c_str());
    const bool clicked = ImGui::ImageButton(name.c_str(), it->second, ImVec2(size, size),
                                            ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint);
    ImGui::PopID();
    return clicked;
}

} // namespace oce::ui
