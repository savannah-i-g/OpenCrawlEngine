cmake_minimum_required(VERSION 3.20)

project(OpenCrawlEngine
    VERSION 0.1.0
    DESCRIPTION "A local, single-player RPG engine driven by a language-model game master"
    LANGUAGES C CXX)

# --- Language standards ---------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Defaults -------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Options --------------------------------------------------------------
option(OCE_BUILD_UI       "Build the Dear ImGui frontend (needs GLFW/OpenGL)" ON)
option(OCE_BUILD_TESTS    "Build the unit-test suite"                          ON)
option(OCE_SANITIZERS     "Enable ASan/UBSan in Debug builds"                  ON)

# --- Shared warning flags (our code only; third-party is excluded) --------
add_library(oce_warnings INTERFACE)
target_compile_options(oce_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)

# --- Sanitizers (applied globally in Debug so instrumentation is uniform) --
if(OCE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "OpenCrawlEngine: AddressSanitizer + UBSan enabled")
endif()

include(cmake/dependencies.cmake)

if(OCE_BUILD_TESTS)
    enable_testing()
endif()

# --- Modules (each is added once it carries its own CMakeLists.txt) --------
file(GLOB OCE_LIB_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/libs" "${CMAKE_CURRENT_SOURCE_DIR}/libs/*")
list(SORT OCE_LIB_DIRS)
foreach(dir ${OCE_LIB_DIRS})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/${dir}/CMakeLists.txt")
        add_subdirectory(libs/${dir})
    endif()
endforeach()

add_subdirectory(engine)

if(OCE_BUILD_UI)
    add_subdirectory(app)
endif()

message(STATUS "OpenCrawlEngine ${PROJECT_VERSION} | UI=${OCE_BUILD_UI} Tests=${OCE_BUILD_TESTS} Build=${CMAKE_BUILD_TYPE}")
