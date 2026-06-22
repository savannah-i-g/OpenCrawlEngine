# oce_ui — the Dear ImGui frontend library.
#
# Renders engine state and emits user intent. Depends on graphics libraries;
# only built when OCE_BUILD_UI is on.

add_library(oce_ui STATIC
    src/ui_app.cpp
    src/game_panels.cpp)

target_include_directories(oce_ui PUBLIC include)
target_link_libraries(oce_ui
    PRIVATE oce_engine imgui PkgConfig::GLFW3 OpenGL::GL oce_warnings)
