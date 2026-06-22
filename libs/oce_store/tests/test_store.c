#include "oce_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

static void run_crud(oce_store* s) {
    char* j = NULL;

    CHECK(oce_store_char_upsert(s, "c1", "{\"name\":\"hero\",\"v\":1}", 1) == OCE_STORE_OK);
    CHECK(oce_store_char_load(s, "c1", &j) == OCE_STORE_OK);
    CHECK(j != NULL && strstr(j, "hero") != NULL);
    free(j);
    j = NULL;

    // Upsert replaces the row.
    CHECK(oce_store_char_upsert(s, "c1", "{\"name\":\"hero\",\"v\":2}", 2) == OCE_STORE_OK);
    CHECK(oce_store_char_load(s, "c1", &j) == OCE_STORE_OK);
    CHECK(j != NULL && strstr(j, "\"v\":2") != NULL);
    free(j);
    j = NULL;

    char** ids = NULL;
    size_t nids = 0;
    CHECK(oce_store_char_list(s, &ids, &nids) == OCE_STORE_OK);
    CHECK(nids == 1 && strcmp(ids[0], "c1") == 0);
    oce_store_free_strings(ids, nids);

    // Campaign owned by the character.
    CHECK(oce_store_campaign_upsert(s, "camp1", "c1", "{\"name\":\"Adventure\"}", 1) == OCE_STORE_OK);
    CHECK(oce_store_campaign_load(s, "camp1", &j) == OCE_STORE_OK);
    CHECK(j != NULL && strstr(j, "Adventure") != NULL);
    free(j);
    j = NULL;

    char** cids = NULL;
    size_t ncids = 0;
    CHECK(oce_store_campaign_list(s, "c1", &cids, &ncids) == OCE_STORE_OK);
    CHECK(ncids == 1 && strcmp(cids[0], "camp1") == 0);
    oce_store_free_strings(cids, ncids);
    CHECK(oce_store_campaign_list(s, "other", &cids, &ncids) == OCE_STORE_OK);
    CHECK(ncids == 0);
    oce_store_free_strings(cids, ncids);

    // Transcript messages keep insertion order.
    CHECK(oce_store_msg_append(s, "camp1", "user", "player", "hello", 100) == OCE_STORE_OK);
    CHECK(oce_store_msg_append(s, "camp1", "assistant", "narrator", "you see a door", 101) ==
          OCE_STORE_OK);
    oce_store_msg* msgs = NULL;
    size_t nmsgs = 0;
    CHECK(oce_store_msg_list(s, "camp1", &msgs, &nmsgs) == OCE_STORE_OK);
    CHECK(nmsgs == 2);
    if (nmsgs == 2) {
        CHECK(strcmp(msgs[0].content, "hello") == 0);
        CHECK(strcmp(msgs[1].content, "you see a door") == 0);
        CHECK(msgs[1].ts == 101);
    }
    oce_store_free_messages(msgs, nmsgs);

    CHECK(oce_store_char_load(s, "missing", &j) == OCE_STORE_ERR_NOT_FOUND);

    CHECK(oce_store_char_delete(s, "c1") == OCE_STORE_OK);
    CHECK(oce_store_char_load(s, "c1", &j) == OCE_STORE_ERR_NOT_FOUND);
}

static void cleanup_db(const char* path) {
    char buf[256];
    remove(path);
    snprintf(buf, sizeof buf, "%s-wal", path);
    remove(buf);
    snprintf(buf, sizeof buf, "%s-shm", path);
    remove(buf);
}

int main(void) {
    oce_store* mem = oce_store_open(NULL, OCE_STORE_MEMORY);
    CHECK(mem != NULL);
    CHECK(oce_store_schema_version(mem) == 1);
    run_crud(mem);
    oce_store_close(mem);

    const char* path = "/tmp/oce_store_test.db";
    cleanup_db(path);
    oce_store* sq = oce_store_open(path, OCE_STORE_SQLITE);
    CHECK(sq != NULL);
    CHECK(oce_store_schema_version(sq) == 1);
    run_crud(sq);

    // Persistence across a close/reopen.
    CHECK(oce_store_char_upsert(sq, "persist", "{\"kept\":true}", 1) == OCE_STORE_OK);
    oce_store_close(sq);
    sq = oce_store_open(path, OCE_STORE_SQLITE);
    CHECK(sq != NULL);
    char* j = NULL;
    CHECK(oce_store_char_load(sq, "persist", &j) == OCE_STORE_OK);
    CHECK(j != NULL && strstr(j, "kept") != NULL);
    free(j);
    oce_store_close(sq);
    cleanup_db(path);

    if (failures == 0) {
        printf("oce_store: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "oce_store: %d checks failed\n", failures);
    return 1;
}
