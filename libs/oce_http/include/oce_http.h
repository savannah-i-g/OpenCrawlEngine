#pragma once
// oce_http — HTTP over libcurl, with streaming and a Server-Sent Events parser.
//
// Purpose  : POST requests (buffered or streamed) and an incremental SSE
//            framing parser used to decode model response streams.
// Ownership: handles are single-owner; curl is confined to this module.
// Threading: a handle is not thread-safe; confine it to one thread.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OCE_HTTP_OK = 0,
    OCE_HTTP_ERR_OFFLINE,
    OCE_HTTP_ERR_TIMEOUT,
    OCE_HTTP_ERR_CANCELLED,
    OCE_HTTP_ERR_TLS,
    OCE_HTTP_ERR_INVALID,
    OCE_HTTP_ERR_INTERNAL
} oce_http_status;

// Process-wide curl init; reference-counted. Call once each at startup/shutdown.
bool oce_http_global_init(void);
void oce_http_global_shutdown(void);

typedef struct oce_http oce_http;
oce_http* oce_http_new(void);
void      oce_http_free(oce_http* h);

void oce_http_set_timeouts_ms(oce_http* h, long total_ms, long connect_ms);
void oce_http_set_max_bytes(oce_http* h, size_t max_bytes); // 0 = unlimited
void oce_http_set_user_agent(oce_http* h, const char* ua);
void oce_http_set_tls_verify(oce_http* h, bool verify);

// Streaming POST. `on_data` receives each received chunk; return false to abort
// the transfer (reported as OCE_HTTP_ERR_CANCELLED). On a completed request the
// HTTP status code is written to *status_out and OK is returned regardless of
// that code — inspect it for 4xx/5xx.
typedef bool (*oce_http_data_fn)(const char* data, size_t n, void* user);
oce_http_status oce_http_post_stream(oce_http* h, const char* url,
                                     const char* const* headers, size_t header_count,
                                     const char* body, size_t body_len,
                                     oce_http_data_fn on_data, void* user,
                                     long* status_out);

// --- Server-Sent Events incremental parser --------------------------------
typedef struct {
    const char* event;    // event field, or NULL
    const char* data;     // accumulated data field (NUL-terminated)
    size_t      data_len;
    const char* id;       // id field, or NULL
    bool        is_done;  // true when data is exactly "[DONE]"
} oce_http_sse_event;

// Return false to request the feed stop (propagated by oce_http_sse_feed).
typedef bool (*oce_http_sse_emit_fn)(const oce_http_sse_event* ev, void* user);

typedef struct oce_http_sse oce_http_sse;
oce_http_sse* oce_http_sse_new(oce_http_sse_emit_fn emit, void* user, size_t max_event_bytes);
void          oce_http_sse_free(oce_http_sse* parser);
// Feed bytes. Returns false if emit asked to stop or a buffer cap was exceeded.
bool          oce_http_sse_feed(oce_http_sse* parser, const char* bytes, size_t n);

#ifdef __cplusplus
}
#endif
