#include "oce_json.h"

#include <cjson/cJSON.h>

#include <limits.h>

// oce_json is an opaque alias for cJSON; only pointers cross the boundary.
static inline cJSON* cj(oce_json* n) { return (cJSON*) n; }
static inline const cJSON* ccj(const oce_json* n) { return (const cJSON*) n; }
static inline oce_json* oj(cJSON* n) { return (oce_json*) n; }
static inline const oce_json* coj(const cJSON* n) { return (const oce_json*) n; }

oce_json* oce_json_parse(const char* text, size_t len) {
    if (text == NULL) {
        return NULL;
    }
    return oj(cJSON_ParseWithLength(text, len));
}

void oce_json_free(oce_json* root) {
    cJSON_Delete(cj(root));
}

char* oce_json_print(const oce_json* node, bool pretty) {
    if (node == NULL) {
        return NULL;
    }
    return pretty ? cJSON_Print(ccj(node)) : cJSON_PrintUnformatted(ccj(node));
}

oce_json* oce_json_new_object(void) {
    return oj(cJSON_CreateObject());
}

oce_json* oce_json_new_array(void) {
    return oj(cJSON_CreateArray());
}

bool oce_json_obj_set_str(oce_json* obj, const char* key, const char* value) {
    if (obj == NULL || key == NULL || value == NULL) {
        return false;
    }
    return cJSON_AddStringToObject(cj(obj), key, value) != NULL;
}

bool oce_json_obj_set_int(oce_json* obj, const char* key, long long value) {
    if (obj == NULL || key == NULL) {
        return false;
    }
    return cJSON_AddNumberToObject(cj(obj), key, (double) value) != NULL;
}

bool oce_json_obj_set_double(oce_json* obj, const char* key, double value) {
    if (obj == NULL || key == NULL) {
        return false;
    }
    return cJSON_AddNumberToObject(cj(obj), key, value) != NULL;
}

bool oce_json_obj_set_bool(oce_json* obj, const char* key, bool value) {
    if (obj == NULL || key == NULL) {
        return false;
    }
    return cJSON_AddBoolToObject(cj(obj), key, value) != NULL;
}

bool oce_json_obj_set_null(oce_json* obj, const char* key) {
    if (obj == NULL || key == NULL) {
        return false;
    }
    return cJSON_AddNullToObject(cj(obj), key) != NULL;
}

bool oce_json_obj_set(oce_json* obj, const char* key, oce_json* child) {
    if (obj == NULL || key == NULL || child == NULL) {
        return false;
    }
    return cJSON_AddItemToObject(cj(obj), key, cj(child)) != 0;
}

bool oce_json_arr_append(oce_json* arr, oce_json* child) {
    if (arr == NULL || child == NULL) {
        return false;
    }
    return cJSON_AddItemToArray(cj(arr), cj(child)) != 0;
}

bool oce_json_arr_append_str(oce_json* arr, const char* value) {
    if (arr == NULL || value == NULL) {
        return false;
    }
    cJSON* str = cJSON_CreateString(value);
    if (str == NULL) {
        return false;
    }
    if (cJSON_AddItemToArray(cj(arr), str) == 0) {
        cJSON_Delete(str);
        return false;
    }
    return true;
}

const oce_json* oce_json_get(const oce_json* obj, const char* key) {
    if (obj == NULL || key == NULL) {
        return NULL;
    }
    return coj(cJSON_GetObjectItemCaseSensitive(ccj(obj), key));
}

const char* oce_json_get_str(const oce_json* obj, const char* key, const char* defval) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(ccj(obj), key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return item->valuestring;
    }
    return defval;
}

long long oce_json_get_int(const oce_json* obj, const char* key, long long defval) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(ccj(obj), key);
    if (cJSON_IsNumber(item)) {
        return (long long) item->valuedouble;
    }
    return defval;
}

double oce_json_get_double(const oce_json* obj, const char* key, double defval) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(ccj(obj), key);
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return defval;
}

bool oce_json_get_bool(const oce_json* obj, const char* key, bool defval) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(ccj(obj), key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) != 0;
    }
    return defval;
}

bool oce_json_is_object(const oce_json* node) { return cJSON_IsObject(ccj(node)) != 0; }
bool oce_json_is_array(const oce_json* node) { return cJSON_IsArray(ccj(node)) != 0; }
bool oce_json_is_string(const oce_json* node) { return cJSON_IsString(ccj(node)) != 0; }
bool oce_json_is_number(const oce_json* node) { return cJSON_IsNumber(ccj(node)) != 0; }

size_t oce_json_arr_len(const oce_json* arr) {
    if (!cJSON_IsArray(ccj(arr))) {
        return 0;
    }
    int n = cJSON_GetArraySize(ccj(arr));
    return n > 0 ? (size_t) n : 0;
}

const oce_json* oce_json_arr_at(const oce_json* arr, size_t index) {
    if (!cJSON_IsArray(ccj(arr)) || index > (size_t) INT_MAX) {
        return NULL;
    }
    return coj(cJSON_GetArrayItem(ccj(arr), (int) index));
}

const char* oce_json_as_str(const oce_json* node, const char* defval) {
    const cJSON* n = ccj(node);
    if (cJSON_IsString(n) && n->valuestring != NULL) {
        return n->valuestring;
    }
    return defval;
}

long long oce_json_as_int(const oce_json* node, long long defval) {
    const cJSON* n = ccj(node);
    return cJSON_IsNumber(n) ? (long long) n->valuedouble : defval;
}

double oce_json_as_double(const oce_json* node, double defval) {
    const cJSON* n = ccj(node);
    return cJSON_IsNumber(n) ? n->valuedouble : defval;
}
