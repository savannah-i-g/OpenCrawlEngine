# oce_engine — the headless game library.
#
# Contains the game model, deterministic rules, the game-master tool layer, and
# persistence wiring. It links no UI or graphics libraries, so the whole engine
# stays unit-testable with OCE_BUILD_UI=OFF.

file(GLOB OCE_RULES_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rules/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gm/*.cpp)

add_library(oce_engine STATIC
    src/version.cpp
    src/engine.cpp
    ${OCE_RULES_SRC})

target_include_directories(oce_engine PUBLIC include)
target_link_libraries(oce_engine
    PUBLIC  Threads::Threads oce_agent oce_store
    PRIVATE oce_secrets oce_json oce_warnings)
target_compile_features(oce_engine PUBLIC cxx_std_17)

if(OCE_BUILD_TESTS)
    add_executable(test_engine_smoke tests/test_smoke.cpp)
    target_link_libraries(test_engine_smoke PRIVATE oce_engine oce_warnings)
    add_test(NAME engine_smoke COMMAND test_engine_smoke)

    add_executable(test_engine tests/test_engine.cpp)
    target_link_libraries(test_engine PRIVATE oce_engine oce_warnings)
    add_test(NAME engine COMMAND test_engine)

    add_executable(test_progression tests/test_progression.cpp)
    target_link_libraries(test_progression PRIVATE oce_engine oce_warnings)
    add_test(NAME progression COMMAND test_progression)

    add_executable(test_combat tests/test_combat.cpp)
    target_link_libraries(test_combat PRIVATE oce_engine oce_warnings)
    add_test(NAME combat COMMAND test_combat)

    add_executable(test_items tests/test_items.cpp)
    target_link_libraries(test_items PRIVATE oce_engine oce_warnings)
    add_test(NAME items COMMAND test_items)

    add_executable(test_world tests/test_world.cpp)
    target_link_libraries(test_world PRIVATE oce_engine oce_warnings)
    add_test(NAME world COMMAND test_world)

    add_executable(test_gm tests/test_gm.cpp)
    target_link_libraries(test_gm PRIVATE oce_engine oce_json oce_warnings)
    add_test(NAME gm COMMAND test_gm)
endif()
