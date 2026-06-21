// Liveness check: the engine library links and reports a non-empty version.
#include "oce/version.hpp"

#include <cstdio>
#include <cstring>

int main() {
    const char* v = oce::version_string();
    if (v == nullptr || std::strlen(v) == 0) {
        std::fprintf(stderr, "engine version string is empty\n");
        return 1;
    }
    std::printf("OpenCrawlEngine engine %s\n", v);
    return 0;
}
