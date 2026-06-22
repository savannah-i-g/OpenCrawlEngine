#include "oce_llm.h"

#include "oce_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCE_LLM_MAX_TOOL_CALLS 16

// ---------------------------------------------------------------------------
// Small helpers.
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

static void secure_zero(void* p, size_t n) {
    volatile unsigned char* v = (volatile unsigned char*) p;
    while (n > 0) {
        *v++ = 0;
        --n;
    }
}

// ---------------------------------------------------------------------------
// Request building (pure).
// ---------------------------------------------------------------------------

char* oce_llm_build_request_json(const char* model, const oce_llm_message* msgs, size_t n,
                                 const char* tools_json, bool stream) {
    if (model == NULL || (msgs == NULL && n > 0)) {
        return NULL;
    }
    oce_json* root = oce_json_new_object();
    if (root == NULL) {
        return NULL;
    }
    oce_json_obj_set_str(root, "model", model);

    oce_json* arr = oce_json_new_array();
    for (size_t i = 0; i < n; ++i) {
        oce_json* m = oce_json_new_object();
        oce_json_obj_set_str(m, "role", msgs[i].role ? msgs[i].role : "user");
        if (msgs[i].content != NULL) {
            oce_json_obj_set_str(m, "content", msgs[i].content);
        } else {
            oce_json_obj_set_null(m, "content");
        }
        if (msgs[i].tool_call_id != NULL) {
            oce_json_obj_set_str(m, "tool_call_id", msgs[i].tool_call_id);
        }
        if (msgs[i].tool_calls_json != NULL) {
            oce_json* tcs = oce_json_parse(msgs[i].tool_calls_json, strlen(msgs[i].tool_calls_json));
            if (tcs != NULL) {
                oce_json_obj_set(m, "tool_calls", tcs);
            }
        }
        oce_json_arr_append(arr, m);
    }
    oce_json_obj_set(root, "messages", arr);

    if (stream) {
        oce_json_obj_set_bool(root, "stream", true);
        oce_json* so = oce_json_new_object();
        oce_json_obj_set_bool(so, "include_usage", true);
        oce_json_obj_set(root, "stream_options", so);
    }

    if (tools_json != NULL) {
        oce_json* tools = oce_json_parse(tools_json, strlen(tools_json));
        if (tools != NULL) {
            oce_json_obj_set(root, "tools", tools);
            oce_json_obj_set_str(root, "tool_choice", "auto");
        }
    }

    char* out = oce_json_print(root, false);
    oce_json_free(root);
    return out;
}

// ---------------------------------------------------------------------------
// Streaming decode.
// ---------------------------------------------------------------------------

typedef struct {
    char* id;
    char* name;
    strbuf args;
    bool active;
} tool_accum;

typedef struct {
    const oce_llm_handlers* h;
    tool_accum tools[OCE_LLM_MAX_TOOL_CALLS];
    size_t tool_count_seen;
    char finish_reason[64];
    oce_llm_usage usage;
    bool usage_seen;
    bool cancel;
} stream_state;

static void stream_state_init(stream_state* st, const oce_llm_handlers* h) {
    memset(st, 0, sizeof(*st));
    st->h = h;
}

static void stream_state_free(stream_state* st) {
    for (size_t i = 0; i < OCE_LLM_MAX_TOOL_CALLS; ++i) {
        free(st->tools[i].id);
        free(st->tools[i].name);
        sb_free(&st->tools[i].args);
    }
}

static void decode_tool_calls(stream_state* st, const oce_json* tcs) {
    size_t m = oce_json_arr_len(tcs);
    for (size_t i = 0; i < m; ++i) {
        const oce_json* tc = oce_json_arr_at(tcs, i);
        long long idx = oce_json_get_int(tc, "index", 0);
        if (idx < 0 || idx >= OCE_LLM_MAX_TOOL_CALLS) {
            continue;
        }
        tool_accum* ta = &st->tools[(size_t) idx];
        ta->active = true;
        if ((size_t) (idx + 1) > st->tool_count_seen) {
            st->tool_count_seen = (size_t) (idx + 1);
        }
        const char* id = oce_json_get_str(tc, "id", NULL);
        if (id != NULL && ta->id == NULL) {
            ta->id = dup_str(id);
        }
        const oce_json* fn = oce_json_get(tc, "function");
        if (oce_json_is_object(fn)) {
            const char* nm = oce_json_get_str(fn, "name", NULL);
            if (nm != NULL && ta->name == NULL) {
                ta->name = dup_str(nm);
            }
            const oce_json* args = oce_json_get(fn, "arguments");
            if (oce_json_is_string(args)) {
                const char* a = oce_json_as_str(args, "");
                sb_append(&ta->args, a, strlen(a));
            }
        }
    }
}

static bool llm_on_sse(const oce_http_sse_event* ev, void* user) {
    stream_state* st = (stream_state*) user;

    if (st->h != NULL && st->h->should_cancel != NULL && st->h->should_cancel(st->h->user)) {
        st->cancel = true;
        return false;
    }
    if (ev->is_done) {
        return true;
    }

    oce_json* root = oce_json_parse(ev->data, ev->data_len);
    if (root == NULL) {
        return true; // tolerate keep-alives and non-JSON lines
    }

    const oce_json* usage = oce_json_get(root, "usage");
    if (oce_json_is_object(usage)) {
        st->usage.input_tokens = oce_json_get_int(usage, "prompt_tokens", 0);
        st->usage.output_tokens = oce_json_get_int(usage, "completion_tokens", 0);
        st->usage.total_tokens = oce_json_get_int(usage, "total_tokens", 0);
        st->usage_seen = true;
    }

    const oce_json* choices = oce_json_get(root, "choices");
    if (oce_json_is_array(choices) && oce_json_arr_len(choices) > 0) {
        const oce_json* choice = oce_json_arr_at(choices, 0);
        const oce_json* delta = oce_json_get(choice, "delta");

        const oce_json* content = oce_json_get(delta, "content");
        if (oce_json_is_string(content)) {
            const char* s = oce_json_as_str(content, "");
            if (st->h != NULL && st->h->on_text != NULL && s[0] != '\0') {
                st->h->on_text(s, strlen(s), st->h->user);
            }
        }

        const oce_json* tcs = oce_json_get(delta, "tool_calls");
        if (oce_json_is_array(tcs)) {
            decode_tool_calls(st, tcs);
        }

        const char* fr = oce_json_get_str(choice, "finish_reason", NULL);
        if (fr != NULL) {
            snprintf(st->finish_reason, sizeof(st->finish_reason), "%s", fr);
        }
    }

    oce_json_free(root);
    return true;
}

static void emit_results(stream_state* st, char* fr_out, size_t fr_cap, oce_llm_usage* usage_out) {
    const oce_llm_handlers* h = st->h;
    if (!st->cancel && h != NULL && h->on_tool_call != NULL) {
        for (size_t i = 0; i < st->tool_count_seen; ++i) {
            tool_accum* ta = &st->tools[i];
            if (ta->active) {
                h->on_tool_call(ta->id ? ta->id : "", ta->name ? ta->name : "",
                                ta->args.len ? ta->args.p : "{}", h->user);
            }
        }
    }
    if (fr_out != NULL && fr_cap > 0) {
        snprintf(fr_out, fr_cap, "%s", st->finish_reason);
    }
    if (usage_out != NULL) {
        *usage_out = st->usage;
    }
}

oce_llm_status oce_llm_decode_stream_text(const char* sse_body, size_t len,
                                          const oce_llm_handlers* handlers,
                                          char* finish_reason_out, size_t fr_cap,
                                          oce_llm_usage* usage_out) {
    if (sse_body == NULL) {
        return OCE_LLM_ERR_INVALID;
    }
    stream_state st;
    stream_state_init(&st, handlers);
    oce_http_sse* sse = oce_http_sse_new(llm_on_sse, &st, 0);
    if (sse == NULL) {
        stream_state_free(&st);
        return OCE_LLM_ERR_NOMEM;
    }
    oce_http_sse_feed(sse, sse_body, len);
    oce_http_sse_free(sse);
    emit_results(&st, finish_reason_out, fr_cap, usage_out);
    oce_llm_status result = st.cancel ? OCE_LLM_ERR_CANCELLED : OCE_LLM_OK;
    stream_state_free(&st);
    return result;
}

// ---------------------------------------------------------------------------
// Client handle.
// ---------------------------------------------------------------------------

struct oce_llm {
    char* base_url;
    char* api_key;
    char* model;
    char** extra_headers;
    size_t extra_header_count;
    oce_http* http; // borrowed
    oce_llm_usage last_usage;
    oce_llm_usage total_usage;
};

oce_llm* oce_llm_new(const oce_llm_config* cfg, oce_http* http) {
    if (cfg == NULL || cfg->base_url == NULL || cfg->model == NULL || http == NULL) {
        return NULL;
    }
    oce_llm* llm = calloc(1, sizeof(*llm));
    if (llm == NULL) {
        return NULL;
    }
    llm->http = http;
    llm->base_url = dup_str(cfg->base_url);
    llm->model = dup_str(cfg->model);
    llm->api_key = cfg->api_key ? dup_str(cfg->api_key) : NULL;
    if (llm->base_url == NULL || llm->model == NULL || (cfg->api_key != NULL && llm->api_key == NULL)) {
        oce_llm_free(llm);
        return NULL;
    }
    if (cfg->extra_header_count > 0 && cfg->extra_headers != NULL) {
        llm->extra_headers = calloc(cfg->extra_header_count, sizeof(char*));
        if (llm->extra_headers == NULL) {
            oce_llm_free(llm);
            return NULL;
        }
        llm->extra_header_count = cfg->extra_header_count;
        for (size_t i = 0; i < cfg->extra_header_count; ++i) {
            llm->extra_headers[i] = dup_str(cfg->extra_headers[i]);
            if (llm->extra_headers[i] == NULL) {
                oce_llm_free(llm);
                return NULL;
            }
        }
    }
    return llm;
}

void oce_llm_free(oce_llm* llm) {
    if (llm == NULL) {
        return;
    }
    free(llm->base_url);
    free(llm->model);
    if (llm->api_key != NULL) {
        secure_zero(llm->api_key, strlen(llm->api_key));
        free(llm->api_key);
    }
    if (llm->extra_headers != NULL) {
        for (size_t i = 0; i < llm->extra_header_count; ++i) {
            free(llm->extra_headers[i]);
        }
        free(llm->extra_headers);
    }
    free(llm);
}

static bool http_to_sse(const char* data, size_t n, void* user) {
    return oce_http_sse_feed((oce_http_sse*) user, data, n);
}

oce_llm_status oce_llm_chat_stream(oce_llm* llm, const oce_llm_message* msgs, size_t n,
                                   const char* tools_json, const oce_llm_handlers* handlers,
                                   char* finish_reason_out, size_t fr_cap) {
    if (llm == NULL || (msgs == NULL && n > 0)) {
        return OCE_LLM_ERR_INVALID;
    }

    char* body = oce_llm_build_request_json(llm->model, msgs, n, tools_json, true);
    if (body == NULL) {
        return OCE_LLM_ERR_PARSE;
    }

    // Build the URL: base_url (trailing slashes trimmed) + "/chat/completions".
    size_t bl = strlen(llm->base_url);
    while (bl > 0 && llm->base_url[bl - 1] == '/') {
        --bl;
    }
    size_t url_n = bl + strlen("/chat/completions") + 1;
    char* url = malloc(url_n);

    // Build headers: content-type, accept, optional authorization, extras.
    size_t has_auth = (llm->api_key != NULL) ? 1u : 0u;
    size_t header_count = 2 + has_auth + llm->extra_header_count;
    const char** headers = calloc(header_count, sizeof(char*));
    char* auth = NULL;

    if (url == NULL || headers == NULL) {
        free(url);
        free(headers);
        free(body);
        return OCE_LLM_ERR_NOMEM;
    }
    snprintf(url, url_n, "%.*s/chat/completions", (int) bl, llm->base_url);

    size_t k = 0;
    headers[k++] = "Content-Type: application/json";
    headers[k++] = "Accept: text/event-stream";
    if (has_auth) {
        size_t an = strlen("Authorization: Bearer ") + strlen(llm->api_key) + 1;
        auth = malloc(an);
        if (auth == NULL) {
            free(url);
            free(headers);
            free(body);
            return OCE_LLM_ERR_NOMEM;
        }
        snprintf(auth, an, "Authorization: Bearer %s", llm->api_key);
        headers[k++] = auth;
    }
    for (size_t i = 0; i < llm->extra_header_count; ++i) {
        headers[k++] = llm->extra_headers[i];
    }

    stream_state st;
    stream_state_init(&st, handlers);
    oce_http_sse* sse = oce_http_sse_new(llm_on_sse, &st, 0);
    oce_http_status hs = OCE_HTTP_ERR_INTERNAL;
    long http_status = 0;
    if (sse != NULL) {
        hs = oce_http_post_stream(llm->http, url, headers, header_count, body, strlen(body),
                                  http_to_sse, sse, &http_status);
    }
    oce_http_sse_free(sse);

    emit_results(&st, finish_reason_out, fr_cap, NULL);
    if (st.usage_seen) {
        llm->last_usage = st.usage;
        llm->total_usage.input_tokens += st.usage.input_tokens;
        llm->total_usage.output_tokens += st.usage.output_tokens;
        llm->total_usage.total_tokens += st.usage.total_tokens;
    }

    oce_llm_status result;
    if (st.cancel) {
        result = OCE_LLM_ERR_CANCELLED;
    } else if (hs == OCE_HTTP_OK) {
        result = (http_status >= 200 && http_status < 300) ? OCE_LLM_OK : OCE_LLM_ERR_STATUS;
    } else if (hs == OCE_HTTP_ERR_INVALID) {
        result = OCE_LLM_ERR_INVALID;
    } else {
        result = OCE_LLM_ERR_HTTP;
    }

    stream_state_free(&st);
    if (auth != NULL) {
        secure_zero(auth, strlen(auth));
        free(auth);
    }
    free(headers);
    free(url);
    free(body);
    return result;
}

oce_llm_usage oce_llm_last_usage(const oce_llm* llm) {
    oce_llm_usage zero = {0, 0, 0};
    return llm != NULL ? llm->last_usage : zero;
}

oce_llm_usage oce_llm_total_usage(const oce_llm* llm) {
    oce_llm_usage zero = {0, 0, 0};
    return llm != NULL ? llm->total_usage : zero;
}
