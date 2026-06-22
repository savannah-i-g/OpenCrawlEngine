#include "oce_agent.h"

#include "oce_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCE_AGENT_MAX_TOOLS 32
#define OCE_AGENT_MAX_TOOL_CALLS 16

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------

typedef struct {
    char* p;
    size_t len;
    size_t cap;
} strbuf;

static bool sb_append(strbuf* b, const char* s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) {
            nc *= 2;
        }
        char* np = realloc(b->p, nc);
        if (np == NULL) {
            return false;
        }
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
    return true;
}

static void sb_free(strbuf* b) {
    free(b->p);
    b->p = NULL;
    b->len = 0;
    b->cap = 0;
}

static char* dup_str(const char* s) {
    if (s == NULL) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char* p = malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

// ---------------------------------------------------------------------------
// History and the agent handle.
// ---------------------------------------------------------------------------

typedef struct {
    char* role;
    char* content;
    char* tool_call_id;
    char* tool_calls_json;
} hist_msg;

struct oce_agent {
    oce_agent_backend backend;
    hist_msg* msgs;
    size_t msg_count;
    size_t msg_cap;
    oce_agent_tool tools[OCE_AGENT_MAX_TOOLS];
    size_t tool_count;
    int max_iterations;
};

static bool append_message(oce_agent* ag, const char* role, const char* content,
                           const char* tool_call_id, const char* tool_calls_json) {
    if (ag->msg_count == ag->msg_cap) {
        size_t nc = ag->msg_cap ? ag->msg_cap * 2 : 8;
        hist_msg* nm = realloc(ag->msgs, nc * sizeof(hist_msg));
        if (nm == NULL) {
            return false;
        }
        ag->msgs = nm;
        ag->msg_cap = nc;
    }
    hist_msg* m = &ag->msgs[ag->msg_count];
    m->role = dup_str(role);
    m->content = content ? dup_str(content) : NULL;
    m->tool_call_id = tool_call_id ? dup_str(tool_call_id) : NULL;
    m->tool_calls_json = tool_calls_json ? dup_str(tool_calls_json) : NULL;
    if (m->role == NULL || (content && !m->content) || (tool_call_id && !m->tool_call_id) ||
        (tool_calls_json && !m->tool_calls_json)) {
        free(m->role);
        free(m->content);
        free(m->tool_call_id);
        free(m->tool_calls_json);
        return false;
    }
    ++ag->msg_count;
    return true;
}

static int llm_backend_chat(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                            const oce_llm_handlers* h, char* fr, size_t cap) {
    oce_llm_status s = oce_llm_chat_stream((oce_llm*) ctx, msgs, n, tools_json, h, fr, cap);
    if (s == OCE_LLM_OK) {
        return OCE_AGENT_BACKEND_OK;
    }
    if (s == OCE_LLM_ERR_CANCELLED) {
        return OCE_AGENT_BACKEND_CANCELLED;
    }
    return OCE_AGENT_BACKEND_ERROR;
}

oce_agent_backend oce_agent_backend_llm(oce_llm* llm) {
    oce_agent_backend b = {llm_backend_chat, llm};
    return b;
}

oce_agent* oce_agent_new(oce_agent_backend backend, const char* system_prompt) {
    oce_agent* ag = calloc(1, sizeof(*ag));
    if (ag == NULL) {
        return NULL;
    }
    ag->backend = backend;
    ag->max_iterations = 8;
    if (system_prompt != NULL && !append_message(ag, "system", system_prompt, NULL, NULL)) {
        oce_agent_free(ag);
        return NULL;
    }
    return ag;
}

void oce_agent_free(oce_agent* ag) {
    if (ag == NULL) {
        return;
    }
    for (size_t i = 0; i < ag->msg_count; ++i) {
        free(ag->msgs[i].role);
        free(ag->msgs[i].content);
        free(ag->msgs[i].tool_call_id);
        free(ag->msgs[i].tool_calls_json);
    }
    free(ag->msgs);
    free(ag);
}

bool oce_agent_add_tool(oce_agent* ag, const oce_agent_tool* tool) {
    if (ag == NULL || tool == NULL || tool->name == NULL || tool->invoke == NULL) {
        return false;
    }
    if (ag->tool_count >= OCE_AGENT_MAX_TOOLS) {
        return false;
    }
    ag->tools[ag->tool_count++] = *tool;
    return true;
}

void oce_agent_set_max_iterations(oce_agent* ag, int max_iterations) {
    if (ag != NULL && max_iterations >= 1) {
        ag->max_iterations = max_iterations;
    }
}

bool oce_agent_seed_message(oce_agent* ag, const char* role, const char* content) {
    if (ag == NULL || role == NULL) {
        return false;
    }
    return append_message(ag, role, content, NULL, NULL);
}

size_t oce_agent_message_count(const oce_agent* ag) {
    return ag != NULL ? ag->msg_count : 0;
}

const char* oce_agent_last_text(const oce_agent* ag) {
    if (ag == NULL) {
        return "";
    }
    for (size_t i = ag->msg_count; i > 0; --i) {
        const hist_msg* m = &ag->msgs[i - 1];
        if (m->role != NULL && strcmp(m->role, "assistant") == 0 && m->content != NULL) {
            return m->content;
        }
    }
    return "";
}

// ---------------------------------------------------------------------------
// The turn loop.
// ---------------------------------------------------------------------------

typedef struct {
    char* id;
    char* name;
    char* args;
} pending_call;

typedef struct {
    const oce_agent_observer* obs;
    oce_agent_cancel* cancel;
    strbuf assistant_text;
    pending_call calls[OCE_AGENT_MAX_TOOL_CALLS];
    size_t call_count;
} turn_ctx;

static void turn_ctx_free(turn_ctx* t) {
    sb_free(&t->assistant_text);
    for (size_t i = 0; i < t->call_count; ++i) {
        free(t->calls[i].id);
        free(t->calls[i].name);
        free(t->calls[i].args);
    }
}

static void agent_on_text(const char* d, size_t n, void* user) {
    turn_ctx* t = (turn_ctx*) user;
    sb_append(&t->assistant_text, d, n);
    if (t->obs != NULL && t->obs->on_text != NULL) {
        t->obs->on_text(d, n, t->obs->user);
    }
}

static void agent_on_tool_call(const char* id, const char* name, const char* args, void* user) {
    turn_ctx* t = (turn_ctx*) user;
    if (t->call_count >= OCE_AGENT_MAX_TOOL_CALLS) {
        return;
    }
    pending_call* c = &t->calls[t->call_count++];
    c->id = dup_str(id);
    c->name = dup_str(name);
    c->args = dup_str(args);
}

static bool agent_should_cancel(void* user) {
    turn_ctx* t = (turn_ctx*) user;
    return t->cancel != NULL && t->cancel->flag != 0;
}

static char* build_tools_json(oce_agent* ag) {
    if (ag->tool_count == 0) {
        return NULL;
    }
    strbuf b = {NULL, 0, 0};
    bool ok = sb_append(&b, "[", 1);
    for (size_t i = 0; i < ag->tool_count && ok; ++i) {
        if (i > 0) {
            ok = sb_append(&b, ",", 1);
        }
        const char* spec = ag->tools[i].spec_json ? ag->tools[i].spec_json : "{}";
        ok = ok && sb_append(&b, spec, strlen(spec));
    }
    ok = ok && sb_append(&b, "]", 1);
    if (!ok) {
        sb_free(&b);
        return NULL;
    }
    return b.p;
}

static char* build_tool_calls_json(const turn_ctx* t) {
    oce_json* arr = oce_json_new_array();
    if (arr == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < t->call_count; ++i) {
        oce_json* tc = oce_json_new_object();
        oce_json_obj_set_str(tc, "id", t->calls[i].id ? t->calls[i].id : "");
        oce_json_obj_set_str(tc, "type", "function");
        oce_json* fn = oce_json_new_object();
        oce_json_obj_set_str(fn, "name", t->calls[i].name ? t->calls[i].name : "");
        oce_json_obj_set_str(fn, "arguments", t->calls[i].args ? t->calls[i].args : "{}");
        oce_json_obj_set(tc, "function", fn);
        oce_json_arr_append(arr, tc);
    }
    char* out = oce_json_print(arr, false);
    oce_json_free(arr);
    return out;
}

static char* dispatch_tool(oce_agent* ag, const char* name, const char* args) {
    for (size_t i = 0; i < ag->tool_count; ++i) {
        if (strcmp(ag->tools[i].name, name) == 0) {
            return ag->tools[i].invoke(args ? args : "{}", ag->tools[i].user);
        }
    }
    return dup_str("{\"ok\":false,\"error\":\"unknown tool\"}");
}

static oce_llm_message* build_llm_messages(oce_agent* ag) {
    oce_llm_message* msgs = calloc(ag->msg_count, sizeof(*msgs));
    if (msgs == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < ag->msg_count; ++i) {
        msgs[i].role = ag->msgs[i].role;
        msgs[i].content = ag->msgs[i].content;
        msgs[i].tool_call_id = ag->msgs[i].tool_call_id;
        msgs[i].tool_calls_json = ag->msgs[i].tool_calls_json;
    }
    return msgs;
}

oce_agent_status oce_agent_run(oce_agent* ag, const char* user_message,
                               const oce_agent_observer* observer, oce_agent_cancel* cancel) {
    if (ag == NULL || user_message == NULL) {
        return OCE_AGENT_ERR_INVALID;
    }
    if (!append_message(ag, "user", user_message, NULL, NULL)) {
        return OCE_AGENT_ERR_NOMEM;
    }

    char* tools_json = build_tools_json(ag);
    oce_agent_status status = OCE_AGENT_OK;
    int iter = 0;

    while (true) {
        if (iter >= ag->max_iterations) {
            status = OCE_AGENT_ERR_LIMIT;
            break;
        }
        ++iter;

        oce_llm_message* msgs = build_llm_messages(ag);
        if (msgs == NULL) {
            status = OCE_AGENT_ERR_NOMEM;
            break;
        }

        turn_ctx t;
        memset(&t, 0, sizeof t);
        t.obs = observer;
        t.cancel = cancel;
        oce_llm_handlers h = {agent_on_text, agent_on_tool_call, agent_should_cancel, &t};
        char finish_reason[64] = {0};

        int backend_status = ag->backend.chat(ag->backend.ctx, msgs, ag->msg_count, tools_json, &h,
                                               finish_reason, sizeof finish_reason);
        free(msgs);

        if (backend_status == OCE_AGENT_BACKEND_CANCELLED || (cancel != NULL && cancel->flag != 0)) {
            status = OCE_AGENT_ERR_CANCELLED;
            turn_ctx_free(&t);
            break;
        }
        if (backend_status != OCE_AGENT_BACKEND_OK) {
            status = OCE_AGENT_ERR_LLM;
            turn_ctx_free(&t);
            break;
        }

        char* tool_calls_json = (t.call_count > 0) ? build_tool_calls_json(&t) : NULL;
        bool appended = append_message(ag, "assistant",
                                       t.assistant_text.p ? t.assistant_text.p : "", NULL,
                                       tool_calls_json);
        free(tool_calls_json);
        if (!appended) {
            status = OCE_AGENT_ERR_NOMEM;
            turn_ctx_free(&t);
            break;
        }

        if (t.call_count == 0) {
            turn_ctx_free(&t);
            break; // model stopped calling tools: the turn is complete
        }

        for (size_t i = 0; i < t.call_count; ++i) {
            char* result = dispatch_tool(ag, t.calls[i].name ? t.calls[i].name : "", t.calls[i].args);
            const char* content = result ? result : "{\"ok\":false,\"error\":\"tool failed\"}";
            if (observer != NULL && observer->on_tool != NULL) {
                observer->on_tool(t.calls[i].name ? t.calls[i].name : "",
                                  t.calls[i].args ? t.calls[i].args : "{}", content, observer->user);
            }
            append_message(ag, "tool", content, t.calls[i].id, NULL);
            free(result);
        }
        turn_ctx_free(&t);
    }

    free(tools_json);
    return status;
}
