#include "run.h"

#include <stdlib.h>
#include <string.h>

static char *copy_bytes(const char *bytes, size_t length) {
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, bytes, length);
        copy[length] = '\0';
    }
    return copy;
}

static int holds_nul(const char *bytes, size_t length) {
    return memchr(bytes, '\0', length) != NULL;
}

static int list_from_json(const weivel_json_value *json, weivel_standard_value *value) {
    size_t count = weivel_json_array_length(json);
    weivel_standard_value *items = count == 0 ? NULL : calloc(count, sizeof *items);
    size_t at;
    value->kind = WEIVEL_STANDARD_VALUE_LIST;
    if (count != 0 && items == NULL) {
        return 0;
    }
    value->items = items;
    for (at = 0; at < count; at++) {
        if (!stub_value_from_json(weivel_json_array_at(json, at), &items[at])) {
            value->count = at + 1;
            return 0;
        }
    }
    value->count = count;
    return 1;
}

static int record_from_json(const weivel_json_value *json, weivel_standard_value *value) {
    size_t count = weivel_json_object_size(json);
    weivel_standard_entry *entries = count == 0 ? NULL : calloc(count, sizeof *entries);
    size_t at;
    value->kind = WEIVEL_STANDARD_VALUE_RECORD;
    if (count != 0 && entries == NULL) {
        return 0;
    }
    value->entries = entries;
    for (at = 0; at < count; at++) {
        const char *key = weivel_json_object_key_at(json, at);
        value->count = at + 1;
        entries[at].key_length = strlen(key);
        entries[at].key = copy_bytes(key, entries[at].key_length);
        if (entries[at].key == NULL ||
            !stub_value_from_json(weivel_json_object_value_at(json, at), &entries[at].value)) {
            return 0;
        }
    }
    value->count = count;
    return 1;
}

int stub_value_from_json(const weivel_json_value *json, weivel_standard_value *value) {
    int boolean;
    memset(value, 0, sizeof *value);
    switch (weivel_json_kind_of(json)) {
    case WEIVEL_JSON_BOOL:
        weivel_json_as_bool(json, &boolean);
        value->kind = WEIVEL_STANDARD_VALUE_BOOLEAN;
        value->boolean = boolean ? 1u : 0u;
        return 1;
    case WEIVEL_JSON_NUMBER:
        value->kind = WEIVEL_STANDARD_VALUE_NUMBER;
        weivel_json_as_double(json, &value->number);
        return 1;
    case WEIVEL_JSON_STRING:
        value->kind = WEIVEL_STANDARD_VALUE_TEXT;
        value->length = weivel_json_string_length(json);
        if (holds_nul(weivel_json_as_string(json), value->length)) {
            return 0;
        }
        value->text = copy_bytes(weivel_json_as_string(json), value->length);
        return value->text != NULL;
    case WEIVEL_JSON_ARRAY:
        return list_from_json(json, value);
    case WEIVEL_JSON_OBJECT:
        return record_from_json(json, value);
    case WEIVEL_JSON_NULL:
    default:
        value->kind = WEIVEL_STANDARD_VALUE_NULL;
        return 1;
    }
}

void stub_value_free(weivel_standard_value *value) {
    size_t at;
    if (value->kind == WEIVEL_STANDARD_VALUE_LIST) {
        for (at = 0; at < value->count; at++) {
            stub_value_free((weivel_standard_value *)&value->items[at]);
        }
        free((void *)value->items);
    } else if (value->kind == WEIVEL_STANDARD_VALUE_RECORD) {
        for (at = 0; at < value->count; at++) {
            weivel_standard_entry *entry = (weivel_standard_entry *)&value->entries[at];
            free((void *)entry->key);
            stub_value_free(&entry->value);
        }
        free((void *)value->entries);
    }
    free((void *)value->text);
    memset(value, 0, sizeof *value);
}
