#pragma once
// oce_json — a small wrapper over cJSON.
//
// Purpose  : parse, build, and query JSON without leaking cJSON into callers.
// Ownership: nodes from parse/new_* are owned by the caller and released with
//            oce_json_free (free the root only). Nodes handed to a parent via
//            the set/append functions are owned by that parent. Borrowed
//            accessors (oce_json_get, oce_json_arr_at) return non-owning
//            pointers valid only while the parent tree lives.
// Threading: a node tree is not thread-safe; do not share it across threads.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oce_json oce_json;

// Parsing and printing.
oce_json* oce_json_parse(const char* text, size_t len); // NULL on parse error
void      oce_json_free(oce_json* root);
char*     oce_json_print(const oce_json* node, bool pretty); // malloc'd; free() it

// Construction.
oce_json* oce_json_new_object(void);
oce_json* oce_json_new_array(void);

// Object setters. Scalar setters copy the value; returns false on error.
bool oce_json_obj_set_str(oce_json* obj, const char* key, const char* value);
bool oce_json_obj_set_int(oce_json* obj, const char* key, long long value);
bool oce_json_obj_set_double(oce_json* obj, const char* key, double value);
bool oce_json_obj_set_bool(oce_json* obj, const char* key, bool value);
bool oce_json_obj_set_null(oce_json* obj, const char* key);
// Ownership of `child` transfers to `obj`; do not free child separately.
bool oce_json_obj_set(oce_json* obj, const char* key, oce_json* child);

// Array append. oce_json_arr_append adopts `child`; the str helper copies.
bool oce_json_arr_append(oce_json* arr, oce_json* child);
bool oce_json_arr_append_str(oce_json* arr, const char* value);

// Object queries (borrowed; valid while the parent lives).
const oce_json* oce_json_get(const oce_json* obj, const char* key); // NULL if absent
const char* oce_json_get_str(const oce_json* obj, const char* key, const char* defval);
long long   oce_json_get_int(const oce_json* obj, const char* key, long long defval);
double      oce_json_get_double(const oce_json* obj, const char* key, double defval);
bool        oce_json_get_bool(const oce_json* obj, const char* key, bool defval);

// Type checks (false for a NULL node).
bool oce_json_is_object(const oce_json* node);
bool oce_json_is_array(const oce_json* node);
bool oce_json_is_string(const oce_json* node);
bool oce_json_is_number(const oce_json* node);

// Array access.
size_t          oce_json_arr_len(const oce_json* arr);
const oce_json* oce_json_arr_at(const oce_json* arr, size_t index); // NULL if out of range

// Scalar extraction from a node.
const char* oce_json_as_str(const oce_json* node, const char* defval);
long long   oce_json_as_int(const oce_json* node, long long defval);
double      oce_json_as_double(const oce_json* node, double defval);

#ifdef __cplusplus
}
#endif
