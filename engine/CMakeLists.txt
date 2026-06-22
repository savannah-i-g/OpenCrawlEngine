# oce_engine — the headless game library.
#
# Contains the game model, deterministic rules, the game-master tool layer, and
# persistence wiring. It links no UI or graphics libraries, so the whole engine
# stays unit-testable with OCE_BUILD_UI=OFF.

add_library(oce_engine STATIC
    src/version.cpp
    src/engine.cpp)

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
endif()
