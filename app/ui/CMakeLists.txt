# oce_ui — the Dear ImGui frontend library.
#
# Renders engine state and emits user intent. Depends on graphics libraries;
# only built when OCE_BUILD_UI is on.

add_library(oce_ui STATIC
    src/ui_app.cpp
    src/game_panels.cpp
    src/icon_cache.cpp)

target_include_directories(oce_ui PUBLIC include)
# Absolute path to the committed UI assets (icon SVGs), baked in for dev runs.
target_compile_definitions(oce_ui PRIVATE OCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
# imgui is PUBLIC: oce_ui's public headers (icon_cache.hpp) include imgui.h.
target_link_libraries(oce_ui
    PUBLIC imgui
    PRIVATE oce_engine oce_nanosvg PkgConfig::GLFW3 OpenGL::GL oce_warnings)
