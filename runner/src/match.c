#include "match.h"

#include <string.h>

static const char *const ASSERTIONS[] = {"before", "contains", "equals", "absent", "present", "code", "subject",
                                          "hasFix"};

static int is_present(const weivel_json_value *value) {
    return value != NULL && weivel_json_kind_of(value) != WEIVEL_JSON_NULL;
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

static void render_number(text_builder *out, const weivel_json_value *value) {
    int64_t integer;
    double real = 0;
    if (weivel_json_as_int64(value, &integer)) {
        text_append_format(out, "%lld", (long long)integer);
    } else if (weivel_json_as_double(value, &real)) {
        text_append_format(out, "%g", real);
    }
}

void match_render(text_builder *out, const weivel_json_value *value) {
    size_t at;
    int flag;
    if (value == NULL) {
        text_append(out, "nothing");
        return;
    }
    switch (weivel_json_kind_of(value)) {
    case WEIVEL_JSON_NULL:
        text_append(out, "null");
        break;
    case WEIVEL_JSON_BOOL:
        weivel_json_as_bool(value, &flag);
        text_append(out, flag ? "true" : "false");
        break;
    case WEIVEL_JSON_NUMBER:
        render_number(out, value);
        break;
    case WEIVEL_JSON_STRING:
        render_string(out, weivel_json_as_string(value), weivel_json_string_length(value));
        break;
    case WEIVEL_JSON_ARRAY:
        text_append(out, "[");
        for (at = 0; at < weivel_json_array_length(value); at++) {
            text_append(out, at == 0 ? "" : ",");
            match_render(out, weivel_json_array_at(value, at));
        }
        text_append(out, "]");
        break;
    case WEIVEL_JSON_OBJECT:
        text_append(out, "{");
        for (at = 0; at < weivel_json_object_size(value); at++) {
            text_append(out, at == 0 ? "" : ",");
            render_string(out, weivel_json_object_key_at(value, at), strlen(weivel_json_object_key_at(value, at)));
            text_append(out, ":");
            match_render(out, weivel_json_object_value_at(value, at));
        }
        text_append(out, "}");
        break;
    }
}

static int numbers_equal(const weivel_json_value *expected, const weivel_json_value *actual) {
    int64_t expected_integer;
    int64_t actual_integer;
    double expected_real = 0;
    double actual_real = 0;
    if (weivel_json_as_int64(expected, &expected_integer) && weivel_json_as_int64(actual, &actual_integer)) {
        return expected_integer == actual_integer;
    }
    return weivel_json_as_double(expected, &expected_real) && weivel_json_as_double(actual, &actual_real) &&
           expected_real == actual_real;
}

static int strings_equal(const weivel_json_value *expected, const weivel_json_value *actual) {
    size_t length = weivel_json_string_length(expected);
    return length == weivel_json_string_length(actual) && memcmp(weivel_json_as_string(expected), weivel_json_as_string(actual), length) == 0;
}

static int deep_equal(const weivel_json_value *expected, const weivel_json_value *actual);

static int arrays_equal(const weivel_json_value *expected, const weivel_json_value *actual) {
    size_t at;
    if (weivel_json_array_length(expected) != weivel_json_array_length(actual)) {
        return 0;
    }
    for (at = 0; at < weivel_json_array_length(expected); at++) {
        if (!deep_equal(weivel_json_array_at(expected, at), weivel_json_array_at(actual, at))) {
            return 0;
        }
    }
    return 1;
}

static int objects_equal(const weivel_json_value *expected, const weivel_json_value *actual) {
    size_t at;
    if (weivel_json_object_size(expected) != weivel_json_object_size(actual)) {
        return 0;
    }
    for (at = 0; at < weivel_json_object_size(expected); at++) {
        if (!deep_equal(weivel_json_object_value_at(expected, at), weivel_json_object_get(actual, weivel_json_object_key_at(expected, at)))) {
            return 0;
        }
    }
    return 1;
}

static int deep_equal(const weivel_json_value *expected, const weivel_json_value *actual) {
    int expected_flag;
    int actual_flag;
    if (expected == NULL || actual == NULL || weivel_json_kind_of(expected) != weivel_json_kind_of(actual)) {
        return 0;
    }
    switch (weivel_json_kind_of(expected)) {
    case WEIVEL_JSON_NULL:
        return 1;
    case WEIVEL_JSON_BOOL:
        return weivel_json_as_bool(expected, &expected_flag) && weivel_json_as_bool(actual, &actual_flag) &&
               expected_flag == actual_flag;
    case WEIVEL_JSON_NUMBER:
        return numbers_equal(expected, actual);
    case WEIVEL_JSON_STRING:
        return strings_equal(expected, actual);
    case WEIVEL_JSON_ARRAY:
        return arrays_equal(expected, actual);
    case WEIVEL_JSON_OBJECT:
        return objects_equal(expected, actual);
    }
    return 0;
}

static void say(text_builder *reasons, const char *key, const char *what, const weivel_json_value *expected,
                const weivel_json_value *actual) {
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

static int is_assertion_block(const weivel_json_value *value) {
    size_t at;
    if (weivel_json_kind_of(value) != WEIVEL_JSON_OBJECT) {
        return 0;
    }
    for (at = 0; at < weivel_json_object_size(value); at++) {
        if (is_assertion_name(weivel_json_object_key_at(value, at))) {
            return 1;
        }
    }
    return 0;
}

static int index_of_text(const weivel_json_value *array, const char *text) {
    size_t at;
    for (at = 0; at < weivel_json_array_length(array); at++) {
        const char *candidate = weivel_json_as_string(weivel_json_array_at(array, at));
        if (candidate != NULL && strcmp(candidate, text) == 0) {
            return (int)at;
        }
    }
    return -1;
}

static size_t check_before_pair(const char *key, const weivel_json_value *pair, const weivel_json_value *actual, text_builder *reasons) {
    const char *earlier = weivel_json_as_string(weivel_json_array_at(pair, 0));
    const char *later = weivel_json_as_string(weivel_json_array_at(pair, 1));
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

static size_t check_before(const char *key, const weivel_json_value *pairs, const weivel_json_value *actual, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    if (actual == NULL || weivel_json_kind_of(actual) != WEIVEL_JSON_ARRAY) {
        say(reasons, key, "expected an ordered array", NULL, actual);
        return 1;
    }
    for (at = 0; at < weivel_json_array_length(pairs); at++) {
        failures += check_before_pair(key, weivel_json_array_at(pairs, at), actual, reasons);
    }
    return failures;
}

static int array_has(const weivel_json_value *array, const weivel_json_value *wanted) {
    size_t at;
    for (at = 0; at < weivel_json_array_length(array); at++) {
        if (deep_equal(wanted, weivel_json_array_at(array, at))) {
            return 1;
        }
    }
    return 0;
}

static size_t check_contains(const char *key, const weivel_json_value *wanted, const weivel_json_value *actual, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    if (actual == NULL || weivel_json_kind_of(actual) != WEIVEL_JSON_ARRAY) {
        say(reasons, key, "expected an array", NULL, actual);
        return 1;
    }
    for (at = 0; at < weivel_json_array_length(wanted); at++) {
        if (!array_has(actual, weivel_json_array_at(wanted, at))) {
            text_append_format(reasons, "%s: does not contain ", key);
            match_render(reasons, weivel_json_array_at(wanted, at));
            text_append(reasons, ", it is ");
            match_render(reasons, actual);
            text_append(reasons, "\n");
            failures++;
        }
    }
    return failures;
}

static size_t check_code(const char *key, const weivel_json_value *expected, const weivel_json_value *actual, text_builder *reasons) {
    const char *wanted = weivel_json_as_string(expected);
    const char *carried = weivel_json_as_string(weivel_json_object_get(actual, "code"));
    if (wanted != NULL && carried != NULL && strcmp(wanted, carried) == 0) {
        return 0;
    }
    say(reasons, key, "expected a refusal carrying the code", expected, actual);
    return 1;
}

/** A null subject holds when the refusal names nothing, so its `subject` is absent or null. */
static size_t check_subject(const char *key, const weivel_json_value *expected, const weivel_json_value *actual,
                            text_builder *reasons) {
    const weivel_json_value *carried = weivel_json_object_get(actual, "subject");
    int holds = weivel_json_kind_of(expected) == WEIVEL_JSON_NULL
                    ? weivel_json_kind_of(actual) == WEIVEL_JSON_OBJECT && !is_present(carried)
                    : carried != NULL && deep_equal(expected, carried);
    if (holds) {
        return 0;
    }
    say(reasons, key, "expected a refusal about", expected, actual);
    return 1;
}

static size_t check_has_fix(const char *key, const weivel_json_value *expected, const weivel_json_value *actual,
                            text_builder *reasons) {
    const weivel_json_value *carried = weivel_json_object_get(actual, "hasFix");
    if (carried != NULL && deep_equal(expected, carried)) {
        return 0;
    }
    say(reasons, key, "expected hasFix", expected, actual);
    return 1;
}

static size_t check_absent(const char *key, const weivel_json_value *argument, const weivel_json_value *actual, text_builder *reasons) {
    int required = 0;
    if (weivel_json_as_bool(argument, &required) && required && is_present(actual)) {
        say(reasons, key, "expected none", NULL, actual);
        return 1;
    }
    return 0;
}

static size_t check_present(const char *key, const weivel_json_value *argument, const weivel_json_value *actual, text_builder *reasons) {
    int required = 0;
    if (weivel_json_as_bool(argument, &required) && required && !is_present(actual)) {
        say(reasons, key, "expected a value", NULL, actual);
        return 1;
    }
    return 0;
}

static size_t check_assertion(const char *key, const char *assertion, const weivel_json_value *argument,
                              const weivel_json_value *actual, text_builder *reasons) {
    if (strcmp(assertion, "code") == 0) {
        return check_code(key, argument, actual, reasons);
    }
    if (strcmp(assertion, "subject") == 0) {
        return check_subject(key, argument, actual, reasons);
    }
    if (strcmp(assertion, "hasFix") == 0) {
        return check_has_fix(key, argument, actual, reasons);
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
    if (strcmp(assertion, "present") == 0) {
        return check_present(key, argument, actual, reasons);
    }
    text_append_format(reasons, "%s: unknown assertion %s\n", key, assertion);
    return 1;
}

static size_t check_block(const char *key, const weivel_json_value *block, const weivel_json_value *actual, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    for (at = 0; at < weivel_json_object_size(block); at++) {
        failures += check_assertion(key, weivel_json_object_key_at(block, at), weivel_json_object_value_at(block, at), actual, reasons);
    }
    return failures;
}

static size_t check_key(const char *key, const weivel_json_value *expected, const weivel_json_value *actual, text_builder *reasons) {
    if (weivel_json_kind_of(expected) == WEIVEL_JSON_NULL) {
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

size_t match_then(const weivel_json_value *then, const weivel_json_value *result, text_builder *reasons) {
    size_t at;
    size_t failures = 0;
    for (at = 0; at < weivel_json_object_size(then); at++) {
        const char *key = weivel_json_object_key_at(then, at);
        failures += check_key(key, weivel_json_object_value_at(then, at), weivel_json_object_get(result, key), reasons);
    }
    return failures;
}

static int is_false_flag(const weivel_json_value *value) {
    int flag = 1;
    return weivel_json_as_bool(value, &flag) && !flag;
}

static const char *refuse_block(const weivel_json_value *block) {
    size_t at;
    for (at = 0; at < weivel_json_object_size(block); at++) {
        const char *name = weivel_json_object_key_at(block, at);
        int is_presence = strcmp(name, "present") == 0 || strcmp(name, "absent") == 0;
        if (is_presence && is_false_flag(weivel_json_object_value_at(block, at))) {
            return strcmp(name, "present") == 0 ? "present: false asserts nothing" : "absent: false asserts nothing";
        }
    }
    return NULL;
}

const char *match_refuse_meaningless(const weivel_json_value *then) {
    size_t at;
    for (at = 0; at < weivel_json_object_size(then); at++) {
        const weivel_json_value *value = weivel_json_object_value_at(then, at);
        const char *reason = is_assertion_block(value) ? refuse_block(value) : NULL;
        if (reason != NULL) {
            return reason;
        }
    }
    return NULL;
}
