#include "oce_ui/ui_app.hpp"

#include "oce_ui/asset_paths.hpp"
#include "oce_ui/ui_fonts.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace oce::ui {
namespace {

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

ImFont* try_load_font(const char* const* paths, size_t count, float size) {
    ImGuiIO& io = ImGui::GetIO();
    for (size_t i = 0; i < count; ++i) {
        FILE* f = std::fopen(paths[i], "rb");
        if (f == nullptr) {
            continue;
        }
        std::fclose(f);
        ImFont* font = io.Fonts->AddFontFromFileTTF(paths[i], size);
        if (font != nullptr) {
            return font;
        }
    }
    return nullptr;
}

// Prefer a font bundled next to the binary (the AppImage case, where the host
// /usr/share/fonts is not ours), then fall back to the system search paths.
ImFont* load_face(const std::filesystem::path& bundled, const char* const* system_paths,
                  size_t count, float size) {
    const std::string b = bundled.string();
    const char* one[] = {b.c_str()};
    if (ImFont* f = try_load_font(one, 1, size); f != nullptr) {
        return f;
    }
    return try_load_font(system_paths, count, size);
}

} // namespace

ImFont* g_body_font = nullptr;
ImFont* g_bold_font = nullptr;
ImFont* g_italic_font = nullptr;
ImFont* g_bolditalic_font = nullptr;
ImFont* g_heading_font = nullptr;

void load_fonts() {
    static const char* const regular[] = {
        "/usr/share/fonts/truetype/ibm-plex/IBMPlexSerif-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSerif.ttf",
    };
    static const char* const bold[] = {
        "/usr/share/fonts/truetype/ibm-plex/IBMPlexSerif-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf",
    };
    static const char* const italic[] = {
        "/usr/share/fonts/truetype/ibm-plex/IBMPlexSerif-Italic.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Italic.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSerif-Italic.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSerifItalic.ttf",
    };
    static const char* const bold_italic[] = {
        "/usr/share/fonts/truetype/ibm-plex/IBMPlexSerif-BoldItalic.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif-BoldItalic.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSerif-BoldItalic.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSerifBoldItalic.ttf",
    };
    const size_t nreg = sizeof regular / sizeof regular[0];
    const size_t nbold = sizeof bold / sizeof bold[0];
    const size_t nital = sizeof italic / sizeof italic[0];
    const size_t nbi = sizeof bold_italic / sizeof bold_italic[0];

    const std::filesystem::path fonts = asset_dir() / "fonts";

    g_body_font = load_face(fonts / "DejaVuSerif.ttf", regular, nreg, 18.0f);
    if (g_body_font == nullptr) {
        g_body_font = ImGui::GetIO().Fonts->AddFontDefault();
    }
    g_bold_font = load_face(fonts / "DejaVuSerif-Bold.ttf", bold, nbold, 18.0f);
    if (g_bold_font == nullptr) {
        g_bold_font = g_body_font;
    }
    g_italic_font = load_face(fonts / "DejaVuSerif-Italic.ttf", italic, nital, 18.0f);
    if (g_italic_font == nullptr) {
        g_italic_font = g_body_font;
    }
    g_bolditalic_font = load_face(fonts / "DejaVuSerif-BoldItalic.ttf", bold_italic, nbi, 18.0f);
    if (g_bolditalic_font == nullptr) {
        g_bolditalic_font = g_bold_font;
    }
    g_heading_font = load_face(fonts / "DejaVuSerif-Bold.ttf", bold, nbold, 26.0f);
    if (g_heading_font == nullptr) {
        g_heading_font = g_body_font;
    }
}

std::unique_ptr<UiApp> UiApp::create(const std::string& title, int width, int height) {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == GLFW_FALSE) {
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Persist the dock layout in the user data dir, not the working directory.
    // The string is static so the pointer stays valid for ImGui's lifetime.
    static const std::string ini_path = (user_data_dir() / "imgui.ini").string();
    io.IniFilename = ini_path.c_str();
    ImGui::StyleColorsDark();
    load_fonts();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::unique_ptr<UiApp> app(new UiApp());
    app->window_ = window;
    return app;
}

UiApp::~UiApp() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool UiApp::begin_frame() {
    if (glfwWindowShouldClose(window_)) {
        return false;
    }
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // The dock host + default layout are built by GamePanels::draw.
    return true;
}

void UiApp::end_frame() {
    ImGui::Render();

    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

} // namespace oce::ui
