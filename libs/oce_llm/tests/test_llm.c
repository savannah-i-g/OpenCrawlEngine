#include "oce_llm.h"

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

static void test_build_request(void) {
    oce_llm_message msgs[2] = {
        {"system", "You are a game master.", NULL, NULL},
        {"user", "I attack the goblin.", NULL, NULL},
    };
    const char* tools = "[{\"type\":\"function\",\"function\":{\"name\":\"apply_stat_changes\","
                        "\"parameters\":{\"type\":\"object\"}}}]";
    char* body = oce_llm_build_request_json("test-model", msgs, 2, tools, true);
    CHECK(body != NULL);

    oce_json* root = oce_json_parse(body, strlen(body));
    free(body);
    CHECK(root != NULL);
    CHECK(strcmp(oce_json_get_str(root, "model", ""), "test-model") == 0);
    CHECK(oce_json_get_bool(root, "stream", false) == true);

    const oce_json* so = oce_json_get(root, "stream_options");
    CHECK(oce_json_get_bool(so, "include_usage", false) == true);

    const oce_json* arr = oce_json_get(root, "messages");
    CHECK(oce_json_is_array(arr));
    CHECK(oce_json_arr_len(arr) == 2);
    CHECK(strcmp(oce_json_get_str(oce_json_arr_at(arr, 0), "role", ""), "system") == 0);
    CHECK(strcmp(oce_json_get_str(oce_json_arr_at(arr, 1), "content", ""), "I attack the goblin.") == 0);

    const oce_json* tarr = oce_json_get(root, "tools");
    CHECK(oce_json_is_array(tarr));
    CHECK(oce_json_arr_len(tarr) == 1);
    CHECK(strcmp(oce_json_get_str(root, "tool_choice", ""), "auto") == 0);

    oce_json_free(root);
}

typedef struct {
    char text[256];
    char tool_id[64];
    char tool_name[64];
    char tool_args[128];
    int tool_calls;
} decode_cap;

static void cap_text(const char* d, size_t n, void* u) {
    decode_cap* c = (decode_cap*) u;
    size_t cur = strlen(c->text);
    if (cur + n < sizeof c->text) {
        memcpy(c->text + cur, d, n);
        c->text[cur + n] = '\0';
    }
}

static void cap_tool(const char* id, const char* name, const char* args, void* u) {
    decode_cap* c = (decode_cap*) u;
    snprintf(c->tool_id, sizeof c->tool_id, "%s", id);
    snprintf(c->tool_name, sizeof c->tool_name, "%s", name);
    snprintf(c->tool_args, sizeof c->tool_args, "%s", args);
    ++c->tool_calls;
}

static void test_decode_stream(void) {
    // Content arrives as two deltas; one tool call's arguments arrive across two
    // fragments; a usage chunk and [DONE] close the stream.
    const char* body =
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\" world\"}}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\","
        "\"type\":\"function\",\"function\":{\"name\":\"apply_stat_changes\","
        "\"arguments\":\"{\\\"hp\\\":\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,"
        "\"function\":{\"arguments\":\"-5}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\n"
        "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":5,"
        "\"total_tokens\":15}}\n\n"
        "data: [DONE]\n\n";

    decode_cap cap;
    memset(&cap, 0, sizeof cap);
    oce_llm_handlers h = {cap_text, cap_tool, NULL, &cap};
    char fr[64] = {0};
    oce_llm_usage usage = {0, 0, 0};
    oce_llm_status s = oce_llm_decode_stream_text(body, strlen(body), &h, fr, sizeof fr, &usage);

    CHECK(s == OCE_LLM_OK);
    CHECK(strcmp(cap.text, "Hello world") == 0);
    CHECK(cap.tool_calls == 1);
    CHECK(strcmp(cap.tool_id, "call_1") == 0);
    CHECK(strcmp(cap.tool_name, "apply_stat_changes") == 0);
    CHECK(strcmp(cap.tool_args, "{\"hp\":-5}") == 0); // fragments joined
    CHECK(strcmp(fr, "tool_calls") == 0);
    CHECK(usage.input_tokens == 10);
    CHECK(usage.output_tokens == 5);
    CHECK(usage.total_tokens == 15);
}

int main(void) {
    test_build_request();
    test_decode_stream();
    if (failures == 0) {
        printf("oce_llm: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "oce_llm: %d checks failed\n", failures);
    return 1;
}
