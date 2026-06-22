#pragma once
// oce_secrets — a process-local store for sensitive strings (e.g. API keys).
//
// Purpose  : hold secrets in memory only and scrub them on replace, delete,
//            and close. This module never writes secrets to disk.
// Ownership: the handle owns its entries; open() allocates, close() scrubs then
//            frees. Values copied out through get() are the caller's to scrub.
// Threading: not thread-safe; a single owner coordinates access.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oce_secrets oce_secrets;

typedef enum {
    OCE_SECRETS_OK = 0,
    OCE_SECRETS_ERR_INVALID,
    OCE_SECRETS_ERR_NOT_FOUND,
    OCE_SECRETS_ERR_TOO_LONG,
    OCE_SECRETS_ERR_NOMEM
} oce_secrets_status;

oce_secrets* oce_secrets_open(void);
void         oce_secrets_close(oce_secrets* s); // scrubs all values first

// Store or replace a secret. Any previous value is scrubbed before being lost.
oce_secrets_status oce_secrets_set(oce_secrets* s, const char* key, const char* value);
// Copy a secret into `out` (NUL-terminated). ERR_TOO_LONG if `cap` is too small.
oce_secrets_status oce_secrets_get(const oce_secrets* s, const char* key, char* out, size_t cap);
bool               oce_secrets_has(const oce_secrets* s, const char* key);
oce_secrets_status oce_secrets_delete(oce_secrets* s, const char* key); // scrubs the value

// Load a secret from an environment variable, if it is set.
oce_secrets_status oce_secrets_load_env(oce_secrets* s, const char* key, const char* env_name);

// Overwrite `n` bytes at `p` with zeros in a way the compiler will not elide.
void oce_secrets_zero(void* p, size_t n);

#ifdef __cplusplus
}
#endif
