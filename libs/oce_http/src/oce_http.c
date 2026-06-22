#include "oce_http.h"

#include <curl/curl.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Growable byte buffer (used by the SSE parser).
// ---------------------------------------------------------------------------

typedef struct {
    char* p;
    size_t len;
    size_t cap;
} sse_buf;

static bool buf_reserve(sse_buf* b, size_t need, size_t max) {
    if (need <= b->cap) {
        return true;
    }
    if (need > max) {
        return false;
    }
    size_t newcap = b->cap ? b->cap * 2 : 64;
    if (newcap < need) {
        newcap = need;
    }
    if (newcap > max) {
        newcap = max;
    }
    char* np = realloc(b->p, newcap);
    if (np == NULL) {
        return false;
    }
    b->p = np;
    b->cap = newcap;
    return true;
}

static bool buf_append(sse_buf* b, const char* data, size_t n, size_t max) {
    if (!buf_reserve(b, b->len + n + 1, max)) { // +1 reserves room for a NUL
        return false;
    }
    memcpy(b->p + b->len, data, n);
    b->len += n;
    return true;
}

static bool buf_push(sse_buf* b, char c, size_t max) {
    return buf_append(b, &c, 1, max);
}

static const char* buf_cstr(sse_buf* b) {
    if (b->len == 0) {
        return "";
    }
    b->p[b->len] = '\0'; // safe: buf_append always reserves the extra byte
    return b->p;
}

// ---------------------------------------------------------------------------
// SSE parser.
// ---------------------------------------------------------------------------

struct oce_http_sse {
    sse_buf line;  // current line, without terminator
    sse_buf data;  // accumulated data field
    sse_buf event; // event field
    sse_buf id;    // id field
    bool pending;  // a field has been set since the last dispatch
    size_t max;
    oce_http_sse_emit_fn emit;
    void* user;
};

oce_http_sse* oce_http_sse_new(oce_http_sse_emit_fn emit, void* user, size_t max_event_bytes) {
    oce_http_sse* p = calloc(1, sizeof(*p));
    if (p == NULL) {
        return NULL;
    }
    p->emit = emit;
    p->user = user;
    p->max = max_event_bytes ? max_event_bytes : (8u * 1024u * 1024u);
    return p;
}

void oce_http_sse_free(oce_http_sse* parser) {
    if (parser == NULL) {
        return;
    }
    free(parser->line.p);
    free(parser->data.p);
    free(parser->event.p);
    free(parser->id.p);
    free(parser);
}

static bool field_is(const char* f, size_t flen, const char* name) {
    return strlen(name) == flen && memcmp(f, name, flen) == 0;
}

static bool sse_dispatch(oce_http_sse* p) {
    oce_http_sse_event ev;
    ev.data = buf_cstr(&p->data);
    ev.data_len = p->data.len;
    ev.event = p->event.len ? buf_cstr(&p->event) : NULL;
    ev.id = p->id.len ? buf_cstr(&p->id) : NULL;
    ev.is_done = (p->data.len == 6 && memcmp(ev.data, "[DONE]", 6) == 0);

    bool keep = p->emit ? p->emit(&ev, p->user) : true;

    p->data.len = 0;
    p->event.len = 0;
    p->id.len = 0;
    p->pending = false;
    return keep;
}

static bool sse_process_line(oce_http_sse* p) {
    const char* line = p->line.p;
    size_t len = p->line.len;

    if (len == 0) {
        return p->pending ? sse_dispatch(p) : true;
    }
    if (line[0] == ':') {
        return true; // comment
    }

    size_t colon = len;
    for (size_t i = 0; i < len; ++i) {
        if (line[i] == ':') {
            colon = i;
            break;
        }
    }

    const char* field = line;
    size_t field_len = colon;
    const char* value = "";
    size_t value_len = 0;
    if (colon < len) {
        size_t vstart = colon + 1;
        if (vstart < len && line[vstart] == ' ') {
            ++vstart; // strip one optional leading space
        }
        value = line + vstart;
        value_len = len - vstart;
    }

    if (field_is(field, field_len, "data")) {
        if (p->data.len > 0 && !buf_push(&p->data, '\n', p->max)) {
            return false;
        }
        if (!buf_append(&p->data, value, value_len, p->max)) {
            return false;
        }
        p->pending = true;
    } else if (field_is(field, field_len, "event")) {
        p->event.len = 0;
        if (!buf_append(&p->event, value, value_len, p->max)) {
            return false;
        }
        p->pending = true;
    } else if (field_is(field, field_len, "id")) {
        p->id.len = 0;
        if (!buf_append(&p->id, value, value_len, p->max)) {
            return false;
        }
        p->pending = true;
    }
    // Other fields (retry, etc.) are ignored.
    return true;
}

bool oce_http_sse_feed(oce_http_sse* parser, const char* bytes, size_t n) {
    if (parser == NULL || (bytes == NULL && n > 0)) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        char c = bytes[i];
        if (c == '\r') {
            continue; // normalize CRLF: a following '\n' ends the line
        }
        if (c == '\n') {
            if (!sse_process_line(parser)) {
                return false;
            }
            parser->line.len = 0;
        } else if (!buf_push(&parser->line, c, parser->max)) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// HTTP transport.
// ---------------------------------------------------------------------------

struct oce_http {
    long timeout_total_ms;
    long timeout_connect_ms;
    size_t max_bytes;
    char* user_agent;
    bool tls_verify;
};

static int g_init_count = 0;

bool oce_http_global_init(void) {
    if (g_init_count == 0 && curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        return false;
    }
    ++g_init_count;
    return true;
}

void oce_http_global_shutdown(void) {
    if (g_init_count > 0 && --g_init_count == 0) {
        curl_global_cleanup();
    }
}

oce_http* oce_http_new(void) {
    oce_http* h = calloc(1, sizeof(*h));
    if (h == NULL) {
        return NULL;
    }
    h->timeout_total_ms = 120000;
    h->timeout_connect_ms = 10000;
    h->max_bytes = 0;
    h->tls_verify = true;
    return h;
}

void oce_http_free(oce_http* h) {
    if (h == NULL) {
        return;
    }
    free(h->user_agent);
    free(h);
}

void oce_http_set_timeouts_ms(oce_http* h, long total_ms, long connect_ms) {
    if (h == NULL) {
        return;
    }
    h->timeout_total_ms = total_ms;
    h->timeout_connect_ms = connect_ms;
}

void oce_http_set_max_bytes(oce_http* h, size_t max_bytes) {
    if (h != NULL) {
        h->max_bytes = max_bytes;
    }
}

void oce_http_set_user_agent(oce_http* h, const char* ua) {
    if (h == NULL) {
        return;
    }
    free(h->user_agent);
    h->user_agent = NULL;
    if (ua != NULL) {
        size_t n = strlen(ua) + 1;
        h->user_agent = malloc(n);
        if (h->user_agent != NULL) {
            memcpy(h->user_agent, ua, n);
        }
    }
}

void oce_http_set_tls_verify(oce_http* h, bool verify) {
    if (h != NULL) {
        h->tls_verify = verify;
    }
}

typedef struct {
    oce_http_data_fn on_data;
    void* user;
    size_t max_bytes;
    size_t total;
    bool cancelled;
    bool overflow;
} stream_ctx;

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    stream_ctx* ctx = (stream_ctx*) userdata;
    size_t n = size * nmemb;
    if (ctx->max_bytes > 0 && ctx->total + n > ctx->max_bytes) {
        ctx->overflow = true;
        return 0; // abort transfer
    }
    ctx->total += n;
    if (ctx->on_data != NULL && n > 0 && !ctx->on_data(ptr, n, ctx->user)) {
        ctx->cancelled = true;
        return 0; // abort transfer
    }
    return n;
}

static oce_http_status map_curl_error(CURLcode res) {
    switch (res) {
        case CURLE_OPERATION_TIMEDOUT:
            return OCE_HTTP_ERR_TIMEOUT;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_CONNECT:
            return OCE_HTTP_ERR_OFFLINE;
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_USE_SSL_FAILED:
            return OCE_HTTP_ERR_TLS;
        default:
            return OCE_HTTP_ERR_INTERNAL;
    }
}

oce_http_status oce_http_post_stream(oce_http* h, const char* url,
                                     const char* const* headers, size_t header_count,
                                     const char* body, size_t body_len,
                                     oce_http_data_fn on_data, void* user,
                                     long* status_out) {
    if (h == NULL || url == NULL) {
        return OCE_HTTP_ERR_INVALID;
    }

    CURL* eh = curl_easy_init();
    if (eh == NULL) {
        return OCE_HTTP_ERR_INTERNAL;
    }

    struct curl_slist* hlist = NULL;
    for (size_t i = 0; i < header_count; ++i) {
        struct curl_slist* nl = curl_slist_append(hlist, headers[i]);
        if (nl == NULL) {
            curl_slist_free_all(hlist);
            curl_easy_cleanup(eh);
            return OCE_HTTP_ERR_INTERNAL;
        }
        hlist = nl;
    }

    stream_ctx ctx = {on_data, user, h->max_bytes, 0, false, false};

    curl_easy_setopt(eh, CURLOPT_URL, url);
    curl_easy_setopt(eh, CURLOPT_POST, 1L);
    curl_easy_setopt(eh, CURLOPT_POSTFIELDS, body ? body : "");
    curl_easy_setopt(eh, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t) body_len);
    if (hlist != NULL) {
        curl_easy_setopt(eh, CURLOPT_HTTPHEADER, hlist);
    }
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(eh, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(eh, CURLOPT_TIMEOUT_MS, h->timeout_total_ms);
    curl_easy_setopt(eh, CURLOPT_CONNECTTIMEOUT_MS, h->timeout_connect_ms);
    curl_easy_setopt(eh, CURLOPT_SSL_VERIFYPEER, h->tls_verify ? 1L : 0L);
    curl_easy_setopt(eh, CURLOPT_SSL_VERIFYHOST, h->tls_verify ? 2L : 0L);
    if (h->user_agent != NULL) {
        curl_easy_setopt(eh, CURLOPT_USERAGENT, h->user_agent);
    }

    CURLcode res = curl_easy_perform(eh);

    oce_http_status status;
    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &code);
        if (status_out != NULL) {
            *status_out = code;
        }
        status = OCE_HTTP_OK;
    } else if (ctx.cancelled) {
        status = OCE_HTTP_ERR_CANCELLED;
    } else if (ctx.overflow) {
        status = OCE_HTTP_ERR_INTERNAL;
    } else {
        status = map_curl_error(res);
    }

    curl_slist_free_all(hlist);
    curl_easy_cleanup(eh);
    return status;
}
