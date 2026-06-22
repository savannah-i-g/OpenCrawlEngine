// Tests the SSE parser offline (no network): correct framing, multi-line data,
// [DONE] detection, and invariance to how bytes are chunked.
#include "oce_http.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char data[256];
    char event[64];
    int is_done;
} cap_event;

typedef struct {
    cap_event evs[16];
    size_t count;
} cap_ctx;

static bool on_event(const oce_http_sse_event* ev, void* user) {
    cap_ctx* c = (cap_ctx*) user;
    if (c->count >= 16) {
        return false;
    }
    cap_event* e = &c->evs[c->count++];
    snprintf(e->data, sizeof e->data, "%s", ev->data ? ev->data : "");
    snprintf(e->event, sizeof e->event, "%s", ev->event ? ev->event : "");
    e->is_done = ev->is_done ? 1 : 0;
    return true;
}

static void run(const char* input, size_t chunk, cap_ctx* out) {
    memset(out, 0, sizeof *out);
    oce_http_sse* p = oce_http_sse_new(on_event, out, 1u << 20);
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i += chunk) {
        size_t n = (i + chunk <= len) ? chunk : (len - i);
        oce_http_sse_feed(p, input + i, n);
    }
    oce_http_sse_free(p);
}

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

int main(void) {
    const char* input = ": keep-alive comment\n"
                        "\n"
                        "data: {\"a\":1}\n"
                        "\n"
                        "event: ping\n"
                        "data: hello\n"
                        "data: world\n"
                        "\n"
                        "data: [DONE]\n"
                        "\n";

    cap_ctx whole;
    run(input, strlen(input), &whole);

    CHECK(whole.count == 3);
    CHECK(strcmp(whole.evs[0].data, "{\"a\":1}") == 0);
    CHECK(whole.evs[0].is_done == 0);
    CHECK(strcmp(whole.evs[1].event, "ping") == 0);
    CHECK(strcmp(whole.evs[1].data, "hello\nworld") == 0); // data lines join with \n
    CHECK(whole.evs[2].is_done == 1);
    CHECK(strcmp(whole.evs[2].data, "[DONE]") == 0);

    // Framing must be invariant to chunk boundaries.
    cap_ctx single, threes;
    run(input, 1, &single);
    run(input, 3, &threes);
    CHECK(single.count == whole.count);
    CHECK(threes.count == whole.count);
    for (size_t i = 0; i < whole.count; ++i) {
        CHECK(strcmp(single.evs[i].data, whole.evs[i].data) == 0);
        CHECK(strcmp(single.evs[i].event, whole.evs[i].event) == 0);
        CHECK(single.evs[i].is_done == whole.evs[i].is_done);
        CHECK(strcmp(threes.evs[i].data, whole.evs[i].data) == 0);
    }

    // CRLF terminators are accepted.
    cap_ctx crlf;
    run("data: x\r\n\r\n", 64, &crlf);
    CHECK(crlf.count == 1);
    CHECK(strcmp(crlf.evs[0].data, "x") == 0);

    if (failures == 0) {
        printf("oce_http: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "oce_http: %d checks failed\n", failures);
    return 1;
}
