#include "oce_agent.h"

#include "oce_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

static char* tdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

// A tool that records how often it ran and the last argument it saw.
typedef struct {
    int calls;
    int last_x;
} add_state;

static char* add_invoke(const char* args, void* user) {
    add_state* s = (add_state*) user;
    ++s->calls;
    oce_json* a = oce_json_parse(args, strlen(args));
    s->last_x = (int) oce_json_get_int(a, "x", 0);
    oce_json_free(a);
    return tdup("{\"ok\":true}");
}

typedef struct {
    char text[128];
    int tool_events;
    char last_tool[32];
} obs_cap;

static void ocap_text(const char* d, size_t n, void* u) {
    obs_cap* c = (obs_cap*) u;
    size_t cur = strlen(c->text);
    if (cur + n < sizeof c->text) {
        memcpy(c->text + cur, d, n);
        c->text[cur + n] = '\0';
    }
}

static void ocap_tool(const char* name, const char* args, const char* result, void* u) {
    (void) args;
    (void) result;
    obs_cap* c = (obs_cap*) u;
    ++c->tool_events;
    snprintf(c->last_tool, sizeof c->last_tool, "%s", name);
}

// Replay backend: turn 1 streams text and calls a tool; turn 2 stops.
typedef struct {
    int turn;
} mock_ctx;

static int mock_chat(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                     const oce_llm_handlers* h, char* fr, size_t cap) {
    (void) msgs;
    (void) n;
    (void) tools_json;
    mock_ctx* m = (mock_ctx*) ctx;
    ++m->turn;
    if (m->turn == 1) {
        h->on_text("thinking ", 9, h->user);
        h->on_tool_call("call_a", "add", "{\"x\":5}", h->user);
        snprintf(fr, cap, "tool_calls");
    } else {
        h->on_text("done", 4, h->user);
        snprintf(fr, cap, "stop");
    }
    return OCE_AGENT_BACKEND_OK;
}

// Backend that never stops calling tools (exercises the iteration cap).
static int loop_chat(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                     const oce_llm_handlers* h, char* fr, size_t cap) {
    (void) ctx;
    (void) msgs;
    (void) n;
    (void) tools_json;
    h->on_tool_call("c", "add", "{\"x\":1}", h->user);
    snprintf(fr, cap, "tool_calls");
    return OCE_AGENT_BACKEND_OK;
}

static void test_replay(void) {
    add_state as = {0, 0};
    oce_agent_tool tool = {
        "add",
        "{\"type\":\"function\",\"function\":{\"name\":\"add\",\"parameters\":{\"type\":\"object\"}}}",
        add_invoke, &as};
    mock_ctx mc = {0};
    oce_agent_backend backend = {mock_chat, &mc};
    oce_agent* ag = oce_agent_new(backend, "system prompt");
    CHECK(ag != NULL);
    CHECK(oce_agent_add_tool(ag, &tool));

    obs_cap oc;
    memset(&oc, 0, sizeof oc);
    oce_agent_observer obs = {ocap_text, ocap_tool, &oc};
    oce_agent_cancel cancel = {0};
    oce_agent_status st = oce_agent_run(ag, "go", &obs, &cancel);

    CHECK(st == OCE_AGENT_OK);
    CHECK(as.calls == 1);
    CHECK(as.last_x == 5);             // tool received the streamed arguments
    CHECK(oc.tool_events == 1);
    CHECK(strcmp(oc.last_tool, "add") == 0);
    CHECK(strstr(oc.text, "thinking") != NULL);
    CHECK(strstr(oc.text, "done") != NULL);
    CHECK(strcmp(oce_agent_last_text(ag), "done") == 0);
    // system, user, assistant(tool_calls), tool, assistant(stop) = 5 messages.
    CHECK(oce_agent_message_count(ag) == 5);
    oce_agent_free(ag);
}

static void test_iteration_cap(void) {
    add_state as = {0, 0};
    oce_agent_tool tool = {"add", "{\"type\":\"function\",\"function\":{\"name\":\"add\"}}", add_invoke,
                           &as};
    oce_agent_backend backend = {loop_chat, NULL};
    oce_agent* ag = oce_agent_new(backend, "sys");
    oce_agent_add_tool(ag, &tool);
    oce_agent_set_max_iterations(ag, 3);
    oce_agent_status st = oce_agent_run(ag, "go", NULL, NULL);
    CHECK(st == OCE_AGENT_ERR_LIMIT);
    CHECK(as.calls == 3); // one dispatch per iteration before the cap stops it
    oce_agent_free(ag);
}

int main(void) {
    test_replay();
    test_iteration_cap();
    if (failures == 0) {
        printf("oce_agent: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "oce_agent: %d checks failed\n", failures);
    return 1;
}
