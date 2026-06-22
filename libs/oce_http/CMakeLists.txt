# oce_http — HTTP transport over libcurl plus an SSE parser.

add_library(oce_http STATIC src/oce_http.c)
target_include_directories(oce_http PUBLIC include)
target_link_libraries(oce_http PRIVATE PkgConfig::CURL oce_warnings)

if(OCE_BUILD_TESTS)
    add_executable(test_oce_http tests/test_http.c)
    target_link_libraries(test_oce_http PRIVATE oce_http oce_warnings)
    add_test(NAME oce_http COMMAND test_oce_http)
endif()
