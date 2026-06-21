#pragma once
// UiApp — owns the GLFW window, OpenGL context, and Dear ImGui context.
//
// Ownership : sole owner of the platform window and the ImGui context.
// Lifetime  : constructed through create(); released by the destructor.
// Threading : must be created and driven from one thread — the thread that
//             owns the OpenGL context. Not safe to share across threads.

#include <memory>
#include <string>

struct GLFWwindow;

namespace oce::ui {

class UiApp {
public:
    // Initializes the window, GL context, and ImGui. Returns nullptr when no
    // display or GL context is available (a headless environment).
    static std::unique_ptr<UiApp> create(const std::string& title,
                                         int width = 1280, int height = 720);
    ~UiApp();

    UiApp(const UiApp&) = delete;
    UiApp& operator=(const UiApp&) = delete;

    // Draws one frame. Returns false once the user has requested close.
    bool run_frame();
    bool should_close() const;

private:
    UiApp() = default;

    GLFWwindow* window_ = nullptr;
};

} // namespace oce::ui
