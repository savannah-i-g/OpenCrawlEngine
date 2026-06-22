#include "oce_json.h"

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
    oce_json* obj = oce_json_new_object();
    CHECK(obj != NULL);
    CHECK(oce_json_obj_set_str(obj, "name", "hero"));
    CHECK(oce_json_obj_set_int(obj, "hp", 30));
    CHECK(oce_json_obj_set_double(obj, "ratio", 0.5));
    CHECK(oce_json_obj_set_bool(obj, "alive", true));

    oce_json* items = oce_json_new_array();
    CHECK(oce_json_arr_append_str(items, "sword"));
    CHECK(oce_json_arr_append_str(items, "shield"));
    CHECK(oce_json_obj_set(obj, "items", items)); // adopts `items`

    char* text = oce_json_print(obj, false);
    CHECK(text != NULL);
    oce_json_free(obj);

    oce_json* parsed = oce_json_parse(text, strlen(text));
    free(text);
    CHECK(parsed != NULL);
    CHECK(strcmp(oce_json_get_str(parsed, "name", ""), "hero") == 0);
    CHECK(oce_json_get_int(parsed, "hp", 0) == 30);
    CHECK(oce_json_get_double(parsed, "ratio", 0.0) == 0.5);
    CHECK(oce_json_get_bool(parsed, "alive", false) == true);

    // Defaults for absent or mistyped keys.
    CHECK(oce_json_get_int(parsed, "missing", -1) == -1);
    CHECK(strcmp(oce_json_get_str(parsed, "missing", "def"), "def") == 0);
    CHECK(strcmp(oce_json_get_str(parsed, "hp", "def"), "def") == 0); // number, not string

    const oce_json* arr = oce_json_get(parsed, "items");
    CHECK(oce_json_is_array(arr));
    CHECK(oce_json_arr_len(arr) == 2);
    CHECK(strcmp(oce_json_as_str(oce_json_arr_at(arr, 0), ""), "sword") == 0);
    CHECK(strcmp(oce_json_as_str(oce_json_arr_at(arr, 1), ""), "shield") == 0);
    CHECK(oce_json_arr_at(arr, 5) == NULL);

    oce_json_free(parsed);

    // Malformed input returns NULL rather than crashing.
    CHECK(oce_json_parse("{not valid", 10) == NULL);

    if (failures == 0) {
        printf("oce_json: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "oce_json: %d checks failed\n", failures);
    return 1;
}
