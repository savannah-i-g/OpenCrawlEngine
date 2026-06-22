# oce_agent — single-agent turn loop with tool calling.

add_library(oce_agent STATIC src/oce_agent.c)
target_include_directories(oce_agent PUBLIC include)
target_link_libraries(oce_agent PUBLIC oce_llm PRIVATE oce_json oce_warnings)

if(OCE_BUILD_TESTS)
    add_executable(test_oce_agent tests/test_agent.c)
    target_link_libraries(test_oce_agent PRIVATE oce_agent oce_json oce_warnings)
    add_test(NAME oce_agent COMMAND test_oce_agent)
endif()
