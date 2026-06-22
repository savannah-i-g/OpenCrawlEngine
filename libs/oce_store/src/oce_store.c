#include "oce_store.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCE_STORE_SCHEMA_VERSION 1

typedef enum {
    TBL_CHARACTERS,
    TBL_CAMPAIGNS
} store_table;

typedef struct {
    void (*close)(void* impl);
    oce_store_status (*kv_upsert)(void* impl, store_table t, const char* id, const char* parent,
                                  const char* json, int version);
    oce_store_status (*kv_load)(void* impl, store_table t, const char* id, char** json_out);
    oce_store_status (*kv_list)(void* impl, store_table t, const char* parent, char*** ids_out,
                                size_t* n_out);
    oce_store_status (*kv_delete)(void* impl, store_table t, const char* id);
    oce_store_status (*msg_append)(void* impl, const char* campaign_id, const char* role,
                                   const char* sender, const char* content, long long ts);
    oce_store_status (*msg_list)(void* impl, const char* campaign_id, oce_store_msg** out,
                                 size_t* n_out);
} store_ops;

struct oce_store {
    const store_ops* ops;
    void* impl;
    int schema_version;
};

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

static bool ids_push(char*** arr, size_t* n, size_t* cap, const char* id) {
    if (*n == *cap) {
        size_t nc = *cap ? *cap * 2 : 8;
        char** na = realloc(*arr, nc * sizeof(char*));
        if (na == NULL) {
            return false;
        }
        *arr = na;
        *cap = nc;
    }
    char* copy = dup_str(id);
    if (copy == NULL) {
        return false;
    }
    (*arr)[(*n)++] = copy;
    return true;
}

static const char* table_name(store_table t) {
    return t == TBL_CHARACTERS ? "characters" : "campaigns";
}

// ---------------------------------------------------------------------------
// SQLite backend.
// ---------------------------------------------------------------------------

typedef struct {
    sqlite3* db;
} sqlite_impl;

static void sq_close(void* impl) {
    sqlite_impl* s = (sqlite_impl*) impl;
    if (s != NULL) {
        sqlite3_close(s->db);
        free(s);
    }
}

static oce_store_status sq_kv_upsert(void* impl, store_table t, const char* id, const char* parent,
                                     const char* json, int version) {
    sqlite3* db = ((sqlite_impl*) impl)->db;
    const char* sql =
        (t == TBL_CHARACTERS)
            ? "INSERT INTO characters(id,json,version) VALUES(?1,?2,?3) "
              "ON CONFLICT(id) DO UPDATE SET json=?2,version=?3"
            : "INSERT INTO campaigns(id,character_id,json,version) VALUES(?1,?2,?3,?4) "
              "ON CONFLICT(id) DO UPDATE SET character_id=?2,json=?3,version=?4";
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        return OCE_STORE_ERR_IO;
    }
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    if (t == TBL_CHARACTERS) {
        sqlite3_bind_text(st, 2, json, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, version);
    } else {
        sqlite3_bind_text(st, 2, parent ? parent : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, json, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 4, version);
    }
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? OCE_STORE_OK : OCE_STORE_ERR_IO;
}

static oce_store_status sq_kv_load(void* impl, store_table t, const char* id, char** json_out) {
    sqlite3* db = ((sqlite_impl*) impl)->db;
    char sql[64];
    snprintf(sql, sizeof sql, "SELECT json FROM %s WHERE id=?1", table_name(t));
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        return OCE_STORE_ERR_IO;
    }
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    oce_store_status status;
    if (rc == SQLITE_ROW) {
        *json_out = dup_str((const char*) sqlite3_column_text(st, 0));
        status = *json_out ? OCE_STORE_OK : OCE_STORE_ERR_NOMEM;
    } else {
        status = (rc == SQLITE_DONE) ? OCE_STORE_ERR_NOT_FOUND : OCE_STORE_ERR_IO;
    }
    sqlite3_finalize(st);
    return status;
}

static oce_store_status sq_kv_list(void* impl, store_table t, const char* parent, char*** ids_out,
                                   size_t* n_out) {
    sqlite3* db = ((sqlite_impl*) impl)->db;
    const char* sql;
    if (t == TBL_CHARACTERS) {
        sql = "SELECT id FROM characters ORDER BY id";
    } else if (parent != NULL) {
        sql = "SELECT id FROM campaigns WHERE character_id=?1 ORDER BY id";
    } else {
        sql = "SELECT id FROM campaigns ORDER BY id";
    }
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        return OCE_STORE_ERR_IO;
    }
    if (t == TBL_CAMPAIGNS && parent != NULL) {
        sqlite3_bind_text(st, 1, parent, -1, SQLITE_TRANSIENT);
    }
    char** ids = NULL;
    size_t n = 0;
    size_t cap = 0;
    oce_store_status status = OCE_STORE_OK;
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        if (!ids_push(&ids, &n, &cap, (const char*) sqlite3_column_text(st, 0))) {
            status = OCE_STORE_ERR_NOMEM;
            break;
        }
    }
    if (status == OCE_STORE_OK && rc != SQLITE_DONE) {
        status = OCE_STORE_ERR_IO;
    }
    sqlite3_finalize(st);
    if (status != OCE_STORE_OK) {
        oce_store_free_strings(ids, n);
        return status;
    }
    *ids_out = ids;
    *n_out = n;
    return OCE_STORE_OK;
}

static oce_store_status sq_kv_delete(void* impl, store_table t, const char* id) {
    sqlite3* db = ((sqlite_impl*) impl)->db;
    char sql[64];
    snprintf(sql, sizeof sql, "DELETE FROM %s WHERE id=?1", table_name(t));
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        return OCE_STORE_ERR_IO;
    }
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? OCE_STORE_OK : OCE_STORE_ERR_IO;
}

static oce_store_status sq_msg_append(void* impl, const char* campaign_id, const char* role,
                                      const char* sender, const char* content, long long ts) {
    sqlite3* db = ((sqlite_impl*) impl)->db;
    const char* sql =
        "INSERT INTO messages(campaign_id,idx,role,sender,content,ts) "
        "VALUES(?1,(SELECT COALESCE(MAX(idx),-1)+1 FROM messages WHERE campaign_id=?1),?2,?3,?4,?5)";
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        return OCE_STORE_ERR_IO;
    }
    sqlite3_bind_text(st, 1, campaign_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, sender, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, ts);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? OCE_STORE_OK : OCE_STORE_ERR_IO;
}

static oce_store_status sq_msg_list(void* impl, const char* campaign_id, oce_store_msg** out,
                                    size_t* n_out) {
    sqlite3* db = ((sqlite_impl*) impl)->db;
    const char* sql = "SELECT role,sender,content,ts FROM messages WHERE campaign_id=?1 ORDER BY idx";
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        return OCE_STORE_ERR_IO;
    }
    sqlite3_bind_text(st, 1, campaign_id, -1, SQLITE_TRANSIENT);

    oce_store_msg* arr = NULL;
    size_t n = 0;
    size_t cap = 0;
    oce_store_status status = OCE_STORE_OK;
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        if (n == cap) {
            size_t nc = cap ? cap * 2 : 8;
            oce_store_msg* na = realloc(arr, nc * sizeof(oce_store_msg));
            if (na == NULL) {
                status = OCE_STORE_ERR_NOMEM;
                break;
            }
            arr = na;
            cap = nc;
        }
        arr[n].role = dup_str((const char*) sqlite3_column_text(st, 0));
        arr[n].sender = dup_str((const char*) sqlite3_column_text(st, 1));
        arr[n].content = dup_str((const char*) sqlite3_column_text(st, 2));
        arr[n].ts = sqlite3_column_int64(st, 3);
        ++n;
    }
    if (status == OCE_STORE_OK && rc != SQLITE_DONE) {
        status = OCE_STORE_ERR_IO;
    }
    sqlite3_finalize(st);
    if (status != OCE_STORE_OK) {
        oce_store_free_messages(arr, n);
        return status;
    }
    *out = arr;
    *n_out = n;
    return OCE_STORE_OK;
}

static const store_ops k_sqlite_ops = {
    .close = sq_close,
    .kv_upsert = sq_kv_upsert,
    .kv_load = sq_kv_load,
    .kv_list = sq_kv_list,
    .kv_delete = sq_kv_delete,
    .msg_append = sq_msg_append,
    .msg_list = sq_msg_list,
};

static int sq_read_schema_version(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    int v = 0;
    if (sqlite3_prepare_v2(db, "SELECT value FROM meta WHERE key='schema_version'", -1, &st, NULL) ==
        SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* t = sqlite3_column_text(st, 0);
            if (t != NULL) {
                v = atoi((const char*) t);
            }
        }
    }
    sqlite3_finalize(st);
    return v;
}

static bool sqlite_open(oce_store* s, const char* path) {
    sqlite_impl* impl = calloc(1, sizeof(*impl));
    if (impl == NULL) {
        return false;
    }
    if (sqlite3_open(path ? path : ":memory:", &impl->db) != SQLITE_OK) {
        sqlite3_close(impl->db);
        free(impl);
        return false;
    }
    const char* schema =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT);"
        "CREATE TABLE IF NOT EXISTS characters(id TEXT PRIMARY KEY, json TEXT NOT NULL, "
        "version INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS campaigns(id TEXT PRIMARY KEY, character_id TEXT, "
        "json TEXT NOT NULL, version INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "campaign_id TEXT NOT NULL, idx INTEGER NOT NULL, role TEXT, sender TEXT, content TEXT, "
        "ts INTEGER);"
        "CREATE INDEX IF NOT EXISTS idx_campaigns_char ON campaigns(character_id);"
        "CREATE INDEX IF NOT EXISTS idx_messages_campaign ON messages(campaign_id, idx);";
    if (sqlite3_exec(impl->db, schema, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(impl->db);
        free(impl);
        return false;
    }
    sqlite3_exec(impl->db, "INSERT OR IGNORE INTO meta(key,value) VALUES('schema_version','1')", NULL,
                 NULL, NULL);
    s->ops = &k_sqlite_ops;
    s->impl = impl;
    s->schema_version = sq_read_schema_version(impl->db);
    return true;
}

// ---------------------------------------------------------------------------
// In-memory backend (for tests).
// ---------------------------------------------------------------------------

typedef struct {
    char* id;
    char* parent;
    char* json;
    int version;
} mem_row;

typedef struct {
    char* campaign_id;
    char* role;
    char* sender;
    char* content;
    long long ts;
} mem_msg;

typedef struct {
    mem_row* chars;
    size_t nchars;
    size_t capchars;
    mem_row* camps;
    size_t ncamps;
    size_t capcamps;
    mem_msg* msgs;
    size_t nmsgs;
    size_t capmsgs;
} mem_impl;

static void mem_table(mem_impl* m, store_table t, mem_row*** rows, size_t** n, size_t** cap) {
    if (t == TBL_CHARACTERS) {
        *rows = &m->chars;
        *n = &m->nchars;
        if (cap != NULL) {
            *cap = &m->capchars;
        }
    } else {
        *rows = &m->camps;
        *n = &m->ncamps;
        if (cap != NULL) {
            *cap = &m->capcamps;
        }
    }
}

static void mem_row_free(mem_row* r) {
    free(r->id);
    free(r->parent);
    free(r->json);
}

static void mem_close(void* impl) {
    mem_impl* m = (mem_impl*) impl;
    if (m == NULL) {
        return;
    }
    for (size_t i = 0; i < m->nchars; ++i) {
        mem_row_free(&m->chars[i]);
    }
    for (size_t i = 0; i < m->ncamps; ++i) {
        mem_row_free(&m->camps[i]);
    }
    for (size_t i = 0; i < m->nmsgs; ++i) {
        free(m->msgs[i].campaign_id);
        free(m->msgs[i].role);
        free(m->msgs[i].sender);
        free(m->msgs[i].content);
    }
    free(m->chars);
    free(m->camps);
    free(m->msgs);
    free(m);
}

static oce_store_status mem_kv_upsert(void* impl, store_table t, const char* id, const char* parent,
                                      const char* json, int version) {
    mem_impl* m = (mem_impl*) impl;
    mem_row** rows;
    size_t* n;
    size_t* cap;
    mem_table(m, t, &rows, &n, &cap);
    for (size_t i = 0; i < *n; ++i) {
        if (strcmp((*rows)[i].id, id) == 0) {
            char* nj = dup_str(json);
            if (nj == NULL) {
                return OCE_STORE_ERR_NOMEM;
            }
            free((*rows)[i].json);
            (*rows)[i].json = nj;
            (*rows)[i].version = version;
            free((*rows)[i].parent);
            (*rows)[i].parent = parent ? dup_str(parent) : NULL;
            return OCE_STORE_OK;
        }
    }
    if (*n == *cap) {
        size_t nc = *cap ? *cap * 2 : 8;
        mem_row* nr = realloc(*rows, nc * sizeof(mem_row));
        if (nr == NULL) {
            return OCE_STORE_ERR_NOMEM;
        }
        *rows = nr;
        *cap = nc;
    }
    mem_row* r = &(*rows)[*n];
    r->id = dup_str(id);
    r->json = dup_str(json);
    r->parent = parent ? dup_str(parent) : NULL;
    r->version = version;
    if (r->id == NULL || r->json == NULL) {
        mem_row_free(r);
        return OCE_STORE_ERR_NOMEM;
    }
    ++*n;
    return OCE_STORE_OK;
}

static oce_store_status mem_kv_load(void* impl, store_table t, const char* id, char** json_out) {
    mem_impl* m = (mem_impl*) impl;
    mem_row** rows;
    size_t* n;
    mem_table(m, t, &rows, &n, NULL);
    for (size_t i = 0; i < *n; ++i) {
        if (strcmp((*rows)[i].id, id) == 0) {
            *json_out = dup_str((*rows)[i].json);
            return *json_out ? OCE_STORE_OK : OCE_STORE_ERR_NOMEM;
        }
    }
    return OCE_STORE_ERR_NOT_FOUND;
}

static oce_store_status mem_kv_list(void* impl, store_table t, const char* parent, char*** ids_out,
                                    size_t* n_out) {
    mem_impl* m = (mem_impl*) impl;
    mem_row** rows;
    size_t* cnt;
    mem_table(m, t, &rows, &cnt, NULL);
    char** ids = NULL;
    size_t n = 0;
    size_t cap = 0;
    for (size_t i = 0; i < *cnt; ++i) {
        if (t == TBL_CAMPAIGNS && parent != NULL &&
            ((*rows)[i].parent == NULL || strcmp((*rows)[i].parent, parent) != 0)) {
            continue;
        }
        if (!ids_push(&ids, &n, &cap, (*rows)[i].id)) {
            oce_store_free_strings(ids, n);
            return OCE_STORE_ERR_NOMEM;
        }
    }
    *ids_out = ids;
    *n_out = n;
    return OCE_STORE_OK;
}

static oce_store_status mem_kv_delete(void* impl, store_table t, const char* id) {
    mem_impl* m = (mem_impl*) impl;
    mem_row** rows;
    size_t* n;
    mem_table(m, t, &rows, &n, NULL);
    for (size_t i = 0; i < *n; ++i) {
        if (strcmp((*rows)[i].id, id) == 0) {
            mem_row_free(&(*rows)[i]);
            (*rows)[i] = (*rows)[*n - 1]; // move the last row into the gap
            --*n;
            return OCE_STORE_OK;
        }
    }
    return OCE_STORE_OK; // idempotent
}

static oce_store_status mem_msg_append(void* impl, const char* campaign_id, const char* role,
                                       const char* sender, const char* content, long long ts) {
    mem_impl* m = (mem_impl*) impl;
    if (m->nmsgs == m->capmsgs) {
        size_t nc = m->capmsgs ? m->capmsgs * 2 : 8;
        mem_msg* na = realloc(m->msgs, nc * sizeof(mem_msg));
        if (na == NULL) {
            return OCE_STORE_ERR_NOMEM;
        }
        m->msgs = na;
        m->capmsgs = nc;
    }
    mem_msg* mm = &m->msgs[m->nmsgs];
    mm->campaign_id = dup_str(campaign_id);
    mm->role = dup_str(role);
    mm->sender = dup_str(sender);
    mm->content = dup_str(content);
    mm->ts = ts;
    ++m->nmsgs;
    return OCE_STORE_OK;
}

static oce_store_status mem_msg_list(void* impl, const char* campaign_id, oce_store_msg** out,
                                     size_t* n_out) {
    mem_impl* m = (mem_impl*) impl;
    oce_store_msg* arr = NULL;
    size_t n = 0;
    size_t cap = 0;
    for (size_t i = 0; i < m->nmsgs; ++i) {
        if (strcmp(m->msgs[i].campaign_id, campaign_id) != 0) {
            continue;
        }
        if (n == cap) {
            size_t nc = cap ? cap * 2 : 8;
            oce_store_msg* na = realloc(arr, nc * sizeof(oce_store_msg));
            if (na == NULL) {
                oce_store_free_messages(arr, n);
                return OCE_STORE_ERR_NOMEM;
            }
            arr = na;
            cap = nc;
        }
        arr[n].role = dup_str(m->msgs[i].role);
        arr[n].sender = dup_str(m->msgs[i].sender);
        arr[n].content = dup_str(m->msgs[i].content);
        arr[n].ts = m->msgs[i].ts;
        ++n;
    }
    *out = arr;
    *n_out = n;
    return OCE_STORE_OK;
}

static const store_ops k_memory_ops = {
    .close = mem_close,
    .kv_upsert = mem_kv_upsert,
    .kv_load = mem_kv_load,
    .kv_list = mem_kv_list,
    .kv_delete = mem_kv_delete,
    .msg_append = mem_msg_append,
    .msg_list = mem_msg_list,
};

static bool memory_open(oce_store* s) {
    mem_impl* impl = calloc(1, sizeof(*impl));
    if (impl == NULL) {
        return false;
    }
    s->ops = &k_memory_ops;
    s->impl = impl;
    s->schema_version = OCE_STORE_SCHEMA_VERSION;
    return true;
}

// ---------------------------------------------------------------------------
// Public dispatch.
// ---------------------------------------------------------------------------

oce_store* oce_store_open(const char* path, oce_store_backend backend) {
    oce_store* s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    bool ok = (backend == OCE_STORE_SQLITE) ? sqlite_open(s, path) : memory_open(s);
    if (!ok) {
        free(s);
        return NULL;
    }
    return s;
}

void oce_store_close(oce_store* s) {
    if (s == NULL) {
        return;
    }
    s->ops->close(s->impl);
    free(s);
}

int oce_store_schema_version(const oce_store* s) {
    return s != NULL ? s->schema_version : 0;
}

oce_store_status oce_store_char_upsert(oce_store* s, const char* id, const char* json, int version) {
    if (s == NULL || id == NULL || json == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_upsert(s->impl, TBL_CHARACTERS, id, NULL, json, version);
}

oce_store_status oce_store_char_load(oce_store* s, const char* id, char** json_out) {
    if (s == NULL || id == NULL || json_out == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_load(s->impl, TBL_CHARACTERS, id, json_out);
}

oce_store_status oce_store_char_list(oce_store* s, char*** ids_out, size_t* n_out) {
    if (s == NULL || ids_out == NULL || n_out == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_list(s->impl, TBL_CHARACTERS, NULL, ids_out, n_out);
}

oce_store_status oce_store_char_delete(oce_store* s, const char* id) {
    if (s == NULL || id == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_delete(s->impl, TBL_CHARACTERS, id);
}

oce_store_status oce_store_campaign_upsert(oce_store* s, const char* id, const char* character_id,
                                           const char* json, int version) {
    if (s == NULL || id == NULL || json == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_upsert(s->impl, TBL_CAMPAIGNS, id, character_id, json, version);
}

oce_store_status oce_store_campaign_load(oce_store* s, const char* id, char** json_out) {
    if (s == NULL || id == NULL || json_out == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_load(s->impl, TBL_CAMPAIGNS, id, json_out);
}

oce_store_status oce_store_campaign_list(oce_store* s, const char* character_id, char*** ids_out,
                                         size_t* n_out) {
    if (s == NULL || ids_out == NULL || n_out == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_list(s->impl, TBL_CAMPAIGNS, character_id, ids_out, n_out);
}

oce_store_status oce_store_campaign_delete(oce_store* s, const char* id) {
    if (s == NULL || id == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->kv_delete(s->impl, TBL_CAMPAIGNS, id);
}

oce_store_status oce_store_msg_append(oce_store* s, const char* campaign_id, const char* role,
                                      const char* sender, const char* content, long long ts) {
    if (s == NULL || campaign_id == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->msg_append(s->impl, campaign_id, role, sender, content, ts);
}

oce_store_status oce_store_msg_list(oce_store* s, const char* campaign_id, oce_store_msg** out,
                                    size_t* n_out) {
    if (s == NULL || campaign_id == NULL || out == NULL || n_out == NULL) {
        return OCE_STORE_ERR_INVALID;
    }
    return s->ops->msg_list(s->impl, campaign_id, out, n_out);
}

void oce_store_free_strings(char** ids, size_t n) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        free(ids[i]);
    }
    free(ids);
}

void oce_store_free_messages(oce_store_msg* msgs, size_t n) {
    if (msgs == NULL) {
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        free(msgs[i].role);
        free(msgs[i].sender);
        free(msgs[i].content);
    }
    free(msgs);
}
