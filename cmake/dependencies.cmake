# External dependencies.
#
# System libraries are located through pkg-config. Dear ImGui ships no build
# system of its own, so it is fetched at a pinned tag and compiled into a
# static `imgui` target here (the docking branch, for multi-panel layouts).

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Infrastructure libraries. Present on the system; linked by the libs/ modules
# from their respective milestones. Located here so configuration fails early
# with a clear message if a development package is missing.
pkg_check_modules(CJSON   REQUIRED IMPORTED_TARGET libcjson)
pkg_check_modules(CURL    REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

if(OCE_BUILD_UI)
    pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)

    include(FetchContent)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.92.8-docking
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    # SYSTEM include so the project's strict warnings do not apply to ImGui.
    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC PkgConfig::GLFW3 OpenGL::GL ${CMAKE_DL_LIBS})

    # nanosvg: header-only SVG parser + rasterizer (zlib license). Fetched and
    # exposed as an include-only target; the implementation is compiled in
    # app/ui/src/icon_cache.cpp. SYSTEM include keeps strict warnings off it.
    FetchContent_Declare(nanosvg
        GIT_REPOSITORY https://github.com/memononen/nanosvg.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE)
    FetchContent_GetProperties(nanosvg)
    if(NOT nanosvg_POPULATED)
        FetchContent_Populate(nanosvg)
    endif()
    add_library(oce_nanosvg INTERFACE)
    target_include_directories(oce_nanosvg SYSTEM INTERFACE ${nanosvg_SOURCE_DIR}/src)
endif()
