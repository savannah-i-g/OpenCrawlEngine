#pragma once
// oce_store — local persistence behind a swappable backend.
//
// Purpose  : durably store characters and campaigns as (id, json, version) rows
//            and append per-campaign transcript messages. A SQLite backend is
//            the default; an in-memory backend stands in for tests.
// Ownership: the store owns its backend. Strings and message arrays returned to
//            the caller are heap-allocated and freed with the free helpers here.
// Threading: a handle is not thread-safe; confine it to one thread.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OCE_STORE_OK = 0,
    OCE_STORE_ERR_INVALID,
    OCE_STORE_ERR_NOT_FOUND,
    OCE_STORE_ERR_IO,
    OCE_STORE_ERR_CORRUPT,
    OCE_STORE_ERR_NOMEM
} oce_store_status;

typedef enum {
    OCE_STORE_SQLITE,
    OCE_STORE_MEMORY
} oce_store_backend;

typedef struct {
    char* role;
    char* sender;
    char* content;
    long long ts;
} oce_store_msg;

// Open a store. For SQLITE, path is the database file (NULL uses an in-process
// database). Creates the schema and runs migrations. NULL on failure.
typedef struct oce_store oce_store;
oce_store* oce_store_open(const char* path, oce_store_backend backend);
void       oce_store_close(oce_store* s);
int        oce_store_schema_version(const oce_store* s);

// Characters (id -> versioned JSON blob).
oce_store_status oce_store_char_upsert(oce_store* s, const char* id, const char* json, int version);
oce_store_status oce_store_char_load(oce_store* s, const char* id, char** json_out);
oce_store_status oce_store_char_list(oce_store* s, char*** ids_out, size_t* n_out);
oce_store_status oce_store_char_delete(oce_store* s, const char* id);

// Campaigns (id -> versioned JSON blob, owned by a character).
oce_store_status oce_store_campaign_upsert(oce_store* s, const char* id, const char* character_id,
                                           const char* json, int version);
oce_store_status oce_store_campaign_load(oce_store* s, const char* id, char** json_out);
oce_store_status oce_store_campaign_list(oce_store* s, const char* character_id, char*** ids_out,
                                         size_t* n_out);
oce_store_status oce_store_campaign_delete(oce_store* s, const char* id);

// Transcript messages (append-only, ordered per campaign).
oce_store_status oce_store_msg_append(oce_store* s, const char* campaign_id, const char* role,
                                      const char* sender, const char* content, long long ts);
oce_store_status oce_store_msg_list(oce_store* s, const char* campaign_id, oce_store_msg** out,
                                    size_t* n_out);

void oce_store_free_strings(char** ids, size_t n);
void oce_store_free_messages(oce_store_msg* msgs, size_t n);

#ifdef __cplusplus
}
#endif
