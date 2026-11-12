#include "match.h"

#include <string.h>

static const char *const ASSERTIONS[] = {"before", "contains", "equals", "absent", "code"};

static int is_present(const json_value *value) {
    return value != NULL && json_kind_of(value) != JSON_NULL;
}

static void render_string(text_builder *out, const char *text, size_t length) {
    size_t at;
    text_append(out, "\"");
    for (at = 0; at < length; at++) {
        unsigned char character = (unsigned char)text[at];
        if (character == '"' || character == '\\') {
            text_append_format(out, "\\%c", character);
        } else if (character < 0x20) {
            text_append_format(out, "\\u%04x", character);
        } else {
            text_append_bytes(out, text + at, 1);
        }
    }
    text_append(out, "\"");
}

static void render_number(text_builder *out, const json_value *value) {
    int64_t integer;
    double real = 0;
    if (json_as_int64(value, &integer)) {
        text_append_format(out, "%lld", (long long)integer);
    } else if (json_as_double(value, &real)) {
        text_append_format(out, "%g", real);
    }
}

void match_render(text_builder *out, const json_value *value) {
    size_t at;
    int flag;
    if (value == NULL) {
        text_append(out, "nothing");
        return;
    }
    switch (json_kind_of(value)) {
    case JSON_NULL:
        text_append(out, "null");
        break;
    case JSON_BOOL:
        json_as_bool(value, &flag);
        text_append(out, flag ? "true" : "false");
        break;
    case JSON_NUMBER:
        render_number(out, value);
        break;
    case JSON_STRING:
        render_string(out, json_as_string(value), json_string_length(value));
        break;
    case JSON_ARRAY:
        text_append(out, "[");
        for (at = 0; at < json_array_length(value); at++) {
            text_append(out, at == 0 ? "" : ",");
            match_render(out, json_array_at(value, at));
        }
        text_append(out, "]");
        break;
    case JSON_OBJECT:
        text_append(out, "{");
        for (at = 0; at < json_object_size(value); at++) {
            text_append(out, at == 0 ? "" : ",");
            render_string(out, json_object_key_at(value, at), strlen(json_object_key_at(value, at)));
            text_append(out, ":");
            match_render(out, json_object_value_at(value, at));
        }
        text_append(out, "}");
        break;
    }
}

static int numbers_equal(const json_value *expected, const json_value *actual) {
    int64_t expected_integer;
    int64_t actual_integer;
    double expected_real = 0;
    double actual_real = 0;
    if (json_as_int64(expected, &expected_integer) && json_as_int64(actual, &actual_integer)) {
        return expected_integer == actual_integer;
    }
    return json_as_double(expected, &expected_real) && json_as_double(actual, &actual_real) &&
           expected_real == actual_real;
}

static int strings_equal(const json_value *expected, const json_value *actual) {
    size_t length = json_string_length(expected);
    return length == json_string_length(actual) && memcmp(json_as_string(expected), json_as_string(actual), length) == 0;
}

static int deep_equal(const json_value *expected, const json_value *actual);

static int arrays_equal(const json_value *expected, const json_value *actual) {
    size_t at;
    if (json_array_length(expected) != json_array_length(actual)) {
        return 0;
    }
    for (at = 0; at < json_array_length(expected); at++) {
        if (!deep_equal(json_array_at(expected, at), json_array_at(actual, at))) {
            return 0;
        }
    }
    return 1;
}

static int objects_equal(const json_value *expected, const json_value *actual) {
    size_t at;
    if (json_object_size(expected) != json_object_size(actual)) {
        return 0;
    }
    for (at = 0; at < json_object_size(expected); at++) {
        if (!deep_equal(json_object_value_at(expected, at), json_object_get(actual, json_object_key_at(expected, at)))) {
            return 0;
        }
    }
    return 1;
}

static int deep_equal(const json_value *expected, const json_value *actual) {
    int expected_flag;
    int actual_flag;
    if (expected == NULL || actual == NULL || json_kind_of(expected) != json_kind_of(actual)) {
        return 0;
    }
    switch (json_kind_of(expected)) {
    case JSON_NULL:
        return 1;
    case JSON_BOOL:
        return json_as_bool(expected, &expected_flag) && json_as_bool(actual, &actual_flag) &&
               expected_flag == actual_flag;
    case JSON_NUMBER:
        return numbers_equal(expected, actual);
    case JSON_STRING:
        return strings_equal(expected, actual);
    case JSON_ARRAY:
        return arrays_equal(expected, actual);
    case JSON_OBJECT:
        return objects_equal(expected, actual);
    }
    return 0;
}

static void say(text_builder *reasons, const char *key, const char *what, const json_value *expected,
                const json_value *actual) {
    text_append_format(reasons, "%s: %s", key, what);
    if (expected != NULL) {
        text_append(reasons, " ");
        match_render(reasons, expected);
    }
    text_append(reasons, ", the adapter answered ");
    match_render(reasons, actual);
    text_append(reasons, "\n");
}

static int is_assertion_name(const char *name) {
    size_t at;
    for (at = 0; at < sizeof ASSERTIONS / sizeof ASSERTIONS[0]; at++) {
        if (strcmp(name, ASSERTIONS[at]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_assertion_block(const json_value *value) {
    size_t at;
    if (json_kind_of(value) != JSON_OBJECT) {
        return 0;
    }
    for (at = 0; at < json_object_size(value); at++) {
        if (is_assertion_name(json_object_key_at(value, at))) {
            return 1;
        }
    }
    return 0;
}

static int index_of_text(const json_value *array, const char *text) {
    size_t at;
    for (at = 0; at < json_array_length(array); at++) {
        const char *candidate = json_as_string(json_array_at(array, at));
        if (candidate != NULL && strcmp(candidate, text) == 0) {
            return (int)at;
        }
    }
    return -1;
}

static size_t check_before_pair(const char *key, const json_value *pair, const json_value *actual, text_builder *reasons) {
    const char *earlier = json_as_string(json_array_at(pair, 0));
    const char *later = json_as_string(json_array_at(pair, 1));
    int earlier_at;
    int later_at;
    if (earlier == NULL || later == NULL) {
        text_append_format(reasons, "%s: a before pair must hold two strings\n", key);
        return 1;
    }
    earlier_at = index_of_text(actual, earlier);
    later_at = index_of_text(actual, later);
    if (earlier_at < 0 || later_at < 0) {
        text_append_format(reasons, "%s: %s before %s cannot be judged, the order is ", key, earlier, later);
        match_render(reasons, actual);
        text_append(reasons, "\n");
        return 1;
    }
    if (earlier_at > later_at) {
        text_append_format(reasons, "%s: %s must come before %s, the order is ", key, earlier, later);
        match_render(reasons, actual);
        text_append(reasons, "\n");
        return 1;
    }
    return 0;
}

static size_t check_before(const char *key, const json_value *pairs, const json_value *actual, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    if (actual == NULL || json_kind_of(actual) != JSON_ARRAY) {
        say(reasons, key, "expected an ordered array", NULL, actual);
        return 1;
    }
    for (at = 0; at < json_array_length(pairs); at++) {
        failures += check_before_pair(key, json_array_at(pairs, at), actual, reasons);
    }
    return failures;
}

static int array_has(const json_value *array, const json_value *wanted) {
    size_t at;
    for (at = 0; at < json_array_length(array); at++) {
        if (deep_equal(wanted, json_array_at(array, at))) {
            return 1;
        }
    }
    return 0;
}

static size_t check_contains(const char *key, const json_value *wanted, const json_value *actual, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    if (actual == NULL || json_kind_of(actual) != JSON_ARRAY) {
        say(reasons, key, "expected an array", NULL, actual);
        return 1;
    }
    for (at = 0; at < json_array_length(wanted); at++) {
        if (!array_has(actual, json_array_at(wanted, at))) {
            text_append_format(reasons, "%s: does not contain ", key);
            match_render(reasons, json_array_at(wanted, at));
            text_append(reasons, ", it is ");
            match_render(reasons, actual);
            text_append(reasons, "\n");
            failures++;
        }
    }
    return failures;
}

static size_t check_code(const char *key, const json_value *expected, const json_value *actual, text_builder *reasons) {
    const char *wanted = json_as_string(expected);
    const char *carried = json_as_string(json_object_get(actual, "code"));
    if (wanted != NULL && carried != NULL && strcmp(wanted, carried) == 0) {
        return 0;
    }
    say(reasons, key, "expected a refusal carrying the code", expected, actual);
    return 1;
}

static size_t check_absent(const char *key, const json_value *argument, const json_value *actual, text_builder *reasons) {
    int required = 0;
    if (json_as_bool(argument, &required) && required && is_present(actual)) {
        say(reasons, key, "expected none", NULL, actual);
        return 1;
    }
    return 0;
}

static size_t check_assertion(const char *key, const char *assertion, const json_value *argument,
                              const json_value *actual, text_builder *reasons) {
    if (strcmp(assertion, "code") == 0) {
        return check_code(key, argument, actual, reasons);
    }
    if (strcmp(assertion, "before") == 0) {
        return check_before(key, argument, actual, reasons);
    }
    if (strcmp(assertion, "contains") == 0) {
        return check_contains(key, argument, actual, reasons);
    }
    if (strcmp(assertion, "equals") == 0) {
        if (deep_equal(argument, actual)) {
            return 0;
        }
        say(reasons, key, "expected", argument, actual);
        return 1;
    }
    if (strcmp(assertion, "absent") == 0) {
        return check_absent(key, argument, actual, reasons);
    }
    text_append_format(reasons, "%s: unknown assertion %s\n", key, assertion);
    return 1;
}

static size_t check_block(const char *key, const json_value *block, const json_value *actual, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    for (at = 0; at < json_object_size(block); at++) {
        failures += check_assertion(key, json_object_key_at(block, at), json_object_value_at(block, at), actual, reasons);
    }
    return failures;
}

static size_t check_key(const char *key, const json_value *expected, const json_value *actual, text_builder *reasons) {
    if (json_kind_of(expected) == JSON_NULL) {
        if (is_present(actual)) {
            say(reasons, key, "expected none", NULL, actual);
            return 1;
        }
        return 0;
    }
    if (is_assertion_block(expected)) {
        return check_block(key, expected, actual, reasons);
    }
    if (!deep_equal(expected, actual)) {
        say(reasons, key, "expected", expected, actual);
        return 1;
    }
    return 0;
}

size_t match_then(const json_value *then, const json_value *result, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    for (at = 0; at < json_object_size(then); at++) {
        const char *key = json_object_key_at(then, at);
        failures += check_key(key, json_object_value_at(then, at), json_object_get(result, key), reasons);
    }
    return failures;
}
