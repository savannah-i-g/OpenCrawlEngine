# oce_secrets — in-memory secret store with secure zeroization.

add_library(oce_secrets STATIC src/oce_secrets.c)
target_include_directories(oce_secrets PUBLIC include)
target_link_libraries(oce_secrets PRIVATE oce_warnings)

if(OCE_BUILD_TESTS)
    add_executable(test_oce_secrets tests/test_secrets.c)
    target_link_libraries(test_oce_secrets PRIVATE oce_secrets oce_warnings)
    add_test(NAME oce_secrets COMMAND test_oce_secrets)
endif()
