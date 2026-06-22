#include "oce_secrets.h"

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

int main(void) {
    oce_secrets* s = oce_secrets_open();
    CHECK(s != NULL);

    char buf[64];
    CHECK(oce_secrets_set(s, "api", "sk-123") == OCE_SECRETS_OK);
    CHECK(oce_secrets_has(s, "api"));
    CHECK(oce_secrets_get(s, "api", buf, sizeof buf) == OCE_SECRETS_OK);
    CHECK(strcmp(buf, "sk-123") == 0);

    // Replace scrubs the old value and stores the new one.
    CHECK(oce_secrets_set(s, "api", "sk-456") == OCE_SECRETS_OK);
    CHECK(oce_secrets_get(s, "api", buf, sizeof buf) == OCE_SECRETS_OK);
    CHECK(strcmp(buf, "sk-456") == 0);

    // Output buffer too small.
    char tiny[3];
    CHECK(oce_secrets_get(s, "api", tiny, sizeof tiny) == OCE_SECRETS_ERR_TOO_LONG);

    // Delete.
    CHECK(oce_secrets_delete(s, "api") == OCE_SECRETS_OK);
    CHECK(!oce_secrets_has(s, "api"));
    CHECK(oce_secrets_get(s, "api", buf, sizeof buf) == OCE_SECRETS_ERR_NOT_FOUND);
    CHECK(oce_secrets_delete(s, "api") == OCE_SECRETS_ERR_NOT_FOUND);

    // Environment bootstrap.
    setenv("OCE_TEST_SECRET", "from-env", 1);
    CHECK(oce_secrets_load_env(s, "k", "OCE_TEST_SECRET") == OCE_SECRETS_OK);
    CHECK(oce_secrets_get(s, "k", buf, sizeof buf) == OCE_SECRETS_OK);
    CHECK(strcmp(buf, "from-env") == 0);
    CHECK(oce_secrets_load_env(s, "k2", "OCE_DOES_NOT_EXIST_12345") == OCE_SECRETS_ERR_NOT_FOUND);

    // Secure zero leaves no residue.
    char scratch[8];
    memset(scratch, 'x', sizeof scratch);
    oce_secrets_zero(scratch, sizeof scratch);
    int all_zero = 1;
    for (size_t i = 0; i < sizeof scratch; ++i) {
        if (scratch[i] != 0) {
            all_zero = 0;
        }
    }
    CHECK(all_zero);

    oce_secrets_close(s);

    if (failures == 0) {
        printf("oce_secrets: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "oce_secrets: %d checks failed\n", failures);
    return 1;
}
