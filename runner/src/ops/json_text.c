#include "json_text.h"

#include <string.h>

static int write_string(const weivel_json_value *value, answer_writer *out) {
    const char *text = weivel_json_as_string(value);
    if (strlen(text) != weivel_json_string_length(value)) {
        return 0;
    }
    answer_string(out, text);
    return 1;
}

static int write_number(const weivel_json_value *value, answer_writer *out) {
    int64_t number;
    if (!weivel_json_as_int64(value, &number)) {
        return 0;
    }
    answer_int(out, number);
    return 1;
}

static int write_bool(const weivel_json_value *value, answer_writer *out) {
    int flag = 0;
    weivel_json_as_bool(value, &flag);
    answer_bool(out, flag);
    return 1;
}

static int write_array(const weivel_json_value *value, answer_writer *out) {
    size_t length = weivel_json_array_length(value);
    size_t at;
    answer_begin_array(out);
    for (at = 0; at < length; at++) {
        if (!json_text_write(weivel_json_array_at(value, at), out)) {
            return 0;
        }
    }
    answer_end_array(out);
    return 1;
}

static int write_object(const weivel_json_value *value, answer_writer *out) {
    size_t size = weivel_json_object_size(value);
    size_t at;
    answer_begin_object(out);
    for (at = 0; at < size; at++) {
        answer_key(out, weivel_json_object_key_at(value, at));
        if (!json_text_write(weivel_json_object_value_at(value, at), out)) {
            return 0;
        }
    }
    answer_end_object(out);
    return 1;
}

static int write_value(const weivel_json_value *value, answer_writer *out) {
    switch (weivel_json_kind_of(value)) {
    case WEIVEL_JSON_NULL:
        answer_null(out);
        return 1;
    case WEIVEL_JSON_BOOL:
        return write_bool(value, out);
    case WEIVEL_JSON_NUMBER:
        return write_number(value, out);
    case WEIVEL_JSON_STRING:
        return write_string(value, out);
    case WEIVEL_JSON_ARRAY:
        return write_array(value, out);
    case WEIVEL_JSON_OBJECT:
        return write_object(value, out);
    default:
        return 0;
    }
}

int json_text_write(const weivel_json_value *value, answer_writer *out) {
    return write_value(value, out) && !out->broken;
}
