# oce_store — local persistence (SQLite + in-memory backends).

add_library(oce_store STATIC src/oce_store.c)
target_include_directories(oce_store PUBLIC include)
target_link_libraries(oce_store PRIVATE PkgConfig::SQLITE3 oce_warnings)

if(OCE_BUILD_TESTS)
    add_executable(test_oce_store tests/test_store.c)
    target_link_libraries(test_oce_store PRIVATE oce_store oce_warnings)
    add_test(NAME oce_store COMMAND test_oce_store)
endif()
