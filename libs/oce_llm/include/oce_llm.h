#pragma once
// oce_llm — an OpenAI-compatible chat client with streaming and tool calls.
//
// Purpose  : build chat-completions requests, stream the response, surface text
//            deltas live, and assemble fragmented tool calls into whole calls.
// Ownership: a handle copies its config (api key included) and borrows the
//            oce_http it is given. The key copy is scrubbed on free.
// Threading: a handle is not thread-safe; confine it to one thread.

#include <stdbool.h>
#include <stddef.h>

#include "oce_http.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OCE_LLM_OK = 0,
    OCE_LLM_ERR_INVALID,
    OCE_LLM_ERR_HTTP,     // transport failure (offline/timeout/TLS)
    OCE_LLM_ERR_STATUS,   // a completed request returned a non-2xx status
    OCE_LLM_ERR_PARSE,    // request could not be built
    OCE_LLM_ERR_CANCELLED,
    OCE_LLM_ERR_NOMEM
} oce_llm_status;

typedef struct {
    const char* base_url;            // e.g. "https://openrouter.ai/api/v1"
    const char* api_key;             // bearer token; NULL for keyless endpoints
    const char* model;               // model id
    const char* const* extra_headers; // array of "Key: Value" strings
    size_t extra_header_count;
} oce_llm_config;

typedef struct {
    const char* role;            // "system" | "user" | "assistant" | "tool"
    const char* content;         // may be NULL
    const char* tool_call_id;    // for "tool" messages, else NULL
    const char* tool_calls_json; // assistant tool_calls array (JSON), else NULL
} oce_llm_message;

typedef struct {
    void (*on_text)(const char* delta, size_t n, void* user);
    void (*on_tool_call)(const char* id, const char* name, const char* args_json, void* user);
    bool (*should_cancel)(void* user);
    void* user;
} oce_llm_handlers;

typedef struct {
    long long input_tokens;
    long long output_tokens;
    long long total_tokens;
} oce_llm_usage;

// Build the chat-completions request body. Pure and offline-testable.
// forced_tool: NULL/empty selects tool_choice "auto"; otherwise the named tool
// is forced. Returns a malloc'd string the caller frees; NULL on error.
char* oce_llm_build_request_json(const char* model, const oce_llm_message* msgs, size_t n,
                                 const char* tools_json, const char* forced_tool, bool stream);

typedef struct oce_llm oce_llm;
oce_llm* oce_llm_new(const oce_llm_config* cfg, oce_http* http); // borrows http
void     oce_llm_free(oce_llm* llm);

// Forces the model to call the named tool on subsequent requests (NULL clears
// it). The name is copied. Intended for one-shot structured calls.
void oce_llm_set_forced_tool(oce_llm* llm, const char* tool_name);

// Stream a chat completion. Text deltas arrive via handlers->on_text; whole
// tool calls are delivered via handlers->on_tool_call after assembly. The
// final finish_reason is written to finish_reason_out (if provided).
oce_llm_status oce_llm_chat_stream(oce_llm* llm, const oce_llm_message* msgs, size_t n,
                                   const char* tools_json, const oce_llm_handlers* handlers,
                                   char* finish_reason_out, size_t fr_cap);

// Decode a recorded SSE response body through the same path as chat_stream,
// without any network. Used for offline replay and testing.
oce_llm_status oce_llm_decode_stream_text(const char* sse_body, size_t len,
                                          const oce_llm_handlers* handlers,
                                          char* finish_reason_out, size_t fr_cap,
                                          oce_llm_usage* usage_out);

oce_llm_usage oce_llm_last_usage(const oce_llm* llm);
oce_llm_usage oce_llm_total_usage(const oce_llm* llm);

#ifdef __cplusplus
}
#endif
