# The application executable: the thin frontend over the engine.

add_subdirectory(ui)

add_executable(OpenCrawlEngine src/main.cpp)
target_link_libraries(OpenCrawlEngine PRIVATE oce_ui oce_engine oce_warnings)
