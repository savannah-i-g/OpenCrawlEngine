#include "oce_secrets.h"

#include <stdlib.h>
#include <string.h>

#define OCE_SECRETS_MAX_ENTRIES 16
#define OCE_SECRETS_MAX_NAME 64
#define OCE_SECRETS_MAX_VALUE 4096

typedef struct {
    char name[OCE_SECRETS_MAX_NAME];
    char value[OCE_SECRETS_MAX_VALUE];
    size_t value_len;
    bool in_use;
} oce_secret_entry;

struct oce_secrets {
    oce_secret_entry entries[OCE_SECRETS_MAX_ENTRIES];
};

void oce_secrets_zero(void* p, size_t n) {
    if (p == NULL) {
        return;
    }
    volatile unsigned char* vp = (volatile unsigned char*) p;
    while (n > 0) {
        *vp++ = 0;
        --n;
    }
}

static int find_index(const oce_secrets* s, const char* key) {
    for (int i = 0; i < OCE_SECRETS_MAX_ENTRIES; ++i) {
        if (s->entries[i].in_use && strcmp(s->entries[i].name, key) == 0) {
            return i;
        }
    }
    return -1;
}

oce_secrets* oce_secrets_open(void) {
    return calloc(1, sizeof(oce_secrets));
}

void oce_secrets_close(oce_secrets* s) {
    if (s == NULL) {
        return;
    }
    for (int i = 0; i < OCE_SECRETS_MAX_ENTRIES; ++i) {
        oce_secrets_zero(s->entries[i].value, OCE_SECRETS_MAX_VALUE);
        oce_secrets_zero(s->entries[i].name, OCE_SECRETS_MAX_NAME);
    }
    free(s);
}

oce_secrets_status oce_secrets_set(oce_secrets* s, const char* key, const char* value) {
    if (s == NULL || key == NULL || value == NULL) {
        return OCE_SECRETS_ERR_INVALID;
    }
    size_t name_len = strlen(key);
    if (name_len == 0 || name_len >= OCE_SECRETS_MAX_NAME) {
        return OCE_SECRETS_ERR_INVALID;
    }
    size_t value_len = strlen(value);
    if (value_len >= OCE_SECRETS_MAX_VALUE) {
        return OCE_SECRETS_ERR_TOO_LONG;
    }

    int idx = find_index(s, key);
    if (idx < 0) {
        for (int i = 0; i < OCE_SECRETS_MAX_ENTRIES; ++i) {
            if (!s->entries[i].in_use) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            return OCE_SECRETS_ERR_NOMEM;
        }
    } else {
        oce_secrets_zero(s->entries[idx].value, OCE_SECRETS_MAX_VALUE);
    }

    oce_secret_entry* e = &s->entries[idx];
    memcpy(e->name, key, name_len + 1);
    memcpy(e->value, value, value_len + 1);
    e->value_len = value_len;
    e->in_use = true;
    return OCE_SECRETS_OK;
}

oce_secrets_status oce_secrets_get(const oce_secrets* s, const char* key, char* out, size_t cap) {
    if (s == NULL || key == NULL || out == NULL) {
        return OCE_SECRETS_ERR_INVALID;
    }
    int idx = find_index(s, key);
    if (idx < 0) {
        return OCE_SECRETS_ERR_NOT_FOUND;
    }
    const oce_secret_entry* e = &s->entries[idx];
    if (cap < e->value_len + 1) {
        return OCE_SECRETS_ERR_TOO_LONG;
    }
    memcpy(out, e->value, e->value_len + 1);
    return OCE_SECRETS_OK;
}

bool oce_secrets_has(const oce_secrets* s, const char* key) {
    if (s == NULL || key == NULL) {
        return false;
    }
    return find_index(s, key) >= 0;
}

oce_secrets_status oce_secrets_delete(oce_secrets* s, const char* key) {
    if (s == NULL || key == NULL) {
        return OCE_SECRETS_ERR_INVALID;
    }
    int idx = find_index(s, key);
    if (idx < 0) {
        return OCE_SECRETS_ERR_NOT_FOUND;
    }
    oce_secret_entry* e = &s->entries[idx];
    oce_secrets_zero(e->value, OCE_SECRETS_MAX_VALUE);
    oce_secrets_zero(e->name, OCE_SECRETS_MAX_NAME);
    e->value_len = 0;
    e->in_use = false;
    return OCE_SECRETS_OK;
}

oce_secrets_status oce_secrets_load_env(oce_secrets* s, const char* key, const char* env_name) {
    if (s == NULL || key == NULL || env_name == NULL) {
        return OCE_SECRETS_ERR_INVALID;
    }
    const char* value = getenv(env_name);
    if (value == NULL) {
        return OCE_SECRETS_ERR_NOT_FOUND;
    }
    return oce_secrets_set(s, key, value);
}
