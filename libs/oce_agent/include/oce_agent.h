#pragma once
// oce_agent — a single-agent perceive/think/act turn loop with tool calling.
//
// Purpose  : own one conversation's history and system prompt; run a user turn
//            end to end — stream the assistant, dispatch each tool call to its
//            registered handler, feed results back, and repeat until the model
//            stops calling tools (or the iteration cap / a cancel is hit).
// Ownership: the agent owns its message history (copied strings). Registered
//            tools' name/spec/user are borrowed and must outlive the agent.
// Threading: a handle is not thread-safe; one thread drives a run at a time.

#include <stdbool.h>
#include <stddef.h>

#include "oce_llm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OCE_AGENT_OK = 0,
    OCE_AGENT_ERR_INVALID,
    OCE_AGENT_ERR_LLM,
    OCE_AGENT_ERR_CANCELLED,
    OCE_AGENT_ERR_LIMIT,
    OCE_AGENT_ERR_NOMEM
} oce_agent_status;

// A model-callable tool. `invoke` parses args_json, acts, and returns a malloc'd
// result string the agent frees (or NULL on failure).
typedef struct {
    const char* name;
    const char* spec_json; // OpenAI function spec (a JSON object)
    char* (*invoke)(const char* args_json, void* user);
    void* user;
} oce_agent_tool;

// A cooperative cancel flag, settable from another thread.
typedef struct {
    volatile int flag;
} oce_agent_cancel;

// Observes a run: streamed text, and each dispatched tool with its result.
typedef struct {
    void (*on_text)(const char* delta, size_t n, void* user);
    void (*on_tool)(const char* name, const char* args_json, const char* result_json, void* user);
    void* user;
} oce_agent_observer;

// The turn source. Live runs wrap oce_llm; tests supply a replay backend.
typedef enum {
    OCE_AGENT_BACKEND_OK = 0,
    OCE_AGENT_BACKEND_CANCELLED,
    OCE_AGENT_BACKEND_ERROR
} oce_agent_backend_status;

typedef struct {
    // Run one model turn: stream text + tool calls through `handlers`, write the
    // finish reason, and return an oce_agent_backend_status.
    int (*chat)(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                const oce_llm_handlers* handlers, char* finish_reason_out, size_t fr_cap);
    void* ctx;
} oce_agent_backend;

// Build a live backend that drives the given client.
oce_agent_backend oce_agent_backend_llm(oce_llm* llm);

typedef struct oce_agent oce_agent;
oce_agent* oce_agent_new(oce_agent_backend backend, const char* system_prompt);
void       oce_agent_free(oce_agent* ag);

bool oce_agent_add_tool(oce_agent* ag, const oce_agent_tool* tool);
void oce_agent_set_max_iterations(oce_agent* ag, int max_iterations);

// Mark a tool as turn-ending: once the model calls it, the agent applies that
// iteration's tools and then stops, handing control back to the caller instead
// of prompting the model again. Use for tools that pass control to the player
// (setting a skill check, starting combat) so the model cannot narrate past the
// unresolved action or loop. May be called more than once; names are copied.
bool oce_agent_add_terminal_tool(oce_agent* ag, const char* name);
bool oce_agent_seed_message(oce_agent* ag, const char* role, const char* content);

oce_agent_status oce_agent_run(oce_agent* ag, const char* user_message,
                               const oce_agent_observer* observer, oce_agent_cancel* cancel);

size_t      oce_agent_message_count(const oce_agent* ag);
const char* oce_agent_last_text(const oce_agent* ag); // last assistant text, or ""

#ifdef __cplusplus
}
#endif
