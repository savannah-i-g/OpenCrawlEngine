# oce_json — JSON parsing, building, and querying over cJSON.

add_library(oce_json STATIC src/oce_json.c)
target_include_directories(oce_json PUBLIC include)
target_link_libraries(oce_json PRIVATE PkgConfig::CJSON oce_warnings)

if(OCE_BUILD_TESTS)
    add_executable(test_oce_json tests/test_json.c)
    target_link_libraries(test_oce_json PRIVATE oce_json oce_warnings)
    add_test(NAME oce_json COMMAND test_oce_json)
endif()
