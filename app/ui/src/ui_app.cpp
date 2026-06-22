#include "oce_ui/ui_app.hpp"

#include "oce_ui/ui_fonts.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <cstdio>

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

} // namespace

ImFont* g_body_font = nullptr;
ImFont* g_bold_font = nullptr;
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
    const size_t nreg = sizeof regular / sizeof regular[0];
    const size_t nbold = sizeof bold / sizeof bold[0];

    g_body_font = try_load_font(regular, nreg, 18.0f);
    if (g_body_font == nullptr) {
        g_body_font = ImGui::GetIO().Fonts->AddFontDefault();
    }
    g_bold_font = try_load_font(bold, nbold, 18.0f);
    if (g_bold_font == nullptr) {
        g_bold_font = g_body_font;
    }
    g_heading_font = try_load_font(bold, nbold, 26.0f);
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
