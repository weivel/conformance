#ifndef CONFORMANCE_JSON_H
#define CONFORMANCE_JSON_H

#include <stddef.h>
#include <stdint.h>

typedef struct json_value json_value;

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_kind;

typedef struct {
    int line;
    int column;
    const char *message;
} json_error;

/**
 * Parses one JSON document. Returns NULL and fills error (when not NULL) on any malformed input;
 * line and column are 1-based. The text need not be NUL-terminated.
 * @implNote A reader only: nothing here ever writes JSON.
 */
json_value *json_parse(const char *text, size_t length, json_error *error);

void json_free(json_value *value);

json_kind json_kind_of(const json_value *value);

/** Returns the member named key, or NULL when value is not an object or has no such member. */
const json_value *json_object_get(const json_value *value, const char *key);

size_t json_object_size(const json_value *value);
const char *json_object_key_at(const json_value *value, size_t index);
const json_value *json_object_value_at(const json_value *value, size_t index);

size_t json_array_length(const json_value *value);
const json_value *json_array_at(const json_value *value, size_t index);

/** Returns the decoded, NUL-terminated string, or NULL when value is not a string. */
const char *json_as_string(const json_value *value);
size_t json_string_length(const json_value *value);

/** Each accessor returns 1 and sets *out when value has that type, else 0. */
int json_as_bool(const json_value *value, int *out);
int json_as_int64(const json_value *value, int64_t *out);
int json_as_double(const json_value *value, double *out);

#endif
