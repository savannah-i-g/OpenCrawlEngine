# oce_llm — OpenAI-compatible chat client with streaming and tool calls.

add_library(oce_llm STATIC src/oce_llm.c)
target_include_directories(oce_llm PUBLIC include)
target_link_libraries(oce_llm PUBLIC oce_http PRIVATE oce_json oce_warnings)

if(OCE_BUILD_TESTS)
    add_executable(test_oce_llm tests/test_llm.c)
    target_link_libraries(test_oce_llm PRIVATE oce_llm oce_json oce_warnings)
    add_test(NAME oce_llm COMMAND test_oce_llm)
endif()
