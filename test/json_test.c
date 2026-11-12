#include "check.h"
#include "json.h"

#include <stdlib.h>

static json_value *parse_text(const char *text) {
    return json_parse(text, strlen(text), NULL);
}

static void check_refused(const char *text, int line, int column, const char *message) {
    json_error error = {0, 0, NULL};
    json_value *value = json_parse(text, strlen(text), &error);
    CHECK(value == NULL);
    check_text(error.message, message, text);
    if (error.line != line || error.column != column) {
        fprintf(stderr, "input %s: expected %d:%d, got %d:%d (%s)\n", text, line, column, error.line, error.column,
                error.message == NULL ? "(null)" : error.message);
    }
    CHECK(error.line == line);
    CHECK(error.column == column);
    json_free(value);
}

static void reads_objects_and_arrays(void) {
    json_value *root = parse_text("{\"name\": \"boot\", \"steps\": [1, 2, 3], \"nested\": {\"empty\": []}}");
    const json_value *steps;
    int64_t second = 0;
    CHECK(root != NULL);
    CHECK(json_kind_of(root) == JSON_OBJECT);
    CHECK(json_object_size(root) == 3);
    check_text(json_object_key_at(root, 1), "steps", "key at 1");
    check_text(json_as_string(json_object_get(root, "name")), "boot", "name");
    CHECK(json_object_get(root, "absent") == NULL);
    steps = json_object_get(root, "steps");
    CHECK(json_array_length(steps) == 3);
    CHECK(json_as_int64(json_array_at(steps, 1), &second) && second == 2);
    CHECK(json_array_at(steps, 3) == NULL);
    CHECK(json_array_length(json_object_get(json_object_get(root, "nested"), "empty")) == 0);
    CHECK(json_object_value_at(root, 0) == json_object_get(root, "name"));
    json_free(root);
}

static void reads_strings_with_escapes(void) {
    json_value *root = parse_text("[\"a\\n\\t\\\"\\\\\\/\", \"\\u00e9\", \"\\ud83d\\ude00\", \"a\\u0000b\"]");
    CHECK(root != NULL);
    check_text(json_as_string(json_array_at(root, 0)), "a\n\t\"\\/", "simple escapes");
    check_text(json_as_string(json_array_at(root, 1)), "\xC3\xA9", "two-byte escape");
    check_text(json_as_string(json_array_at(root, 2)), "\xF0\x9F\x98\x80", "surrogate pair");
    CHECK(json_string_length(json_array_at(root, 3)) == 3);
    json_free(root);
}

static void reads_numbers_as_integer_and_double(void) {
    json_value *root = parse_text("[0, -7, 9223372036854775807, -9223372036854775808, 9223372036854775808, 1.5, 2e3]");
    int64_t integer = 0;
    double real = 0;
    CHECK(root != NULL);
    CHECK(json_as_int64(json_array_at(root, 0), &integer) && integer == 0);
    CHECK(json_as_int64(json_array_at(root, 1), &integer) && integer == -7);
    CHECK(json_as_int64(json_array_at(root, 2), &integer) && integer == INT64_MAX);
    CHECK(json_as_int64(json_array_at(root, 3), &integer) && integer == INT64_MIN);
    CHECK(!json_as_int64(json_array_at(root, 4), &integer));
    CHECK(json_as_double(json_array_at(root, 4), &real) && real > 9.2e18);
    CHECK(!json_as_int64(json_array_at(root, 5), &integer));
    CHECK(json_as_double(json_array_at(root, 5), &real) && real == 1.5);
    CHECK(!json_as_int64(json_array_at(root, 6), &integer));
    CHECK(json_as_double(json_array_at(root, 6), &real) && real == 2000.0);
    CHECK(!json_as_int64(NULL, &integer));
    json_free(root);
}

static void reads_literals(void) {
    json_value *root = parse_text(" [true, false, null] ");
    int flag = -1;
    CHECK(root != NULL);
    CHECK(json_as_bool(json_array_at(root, 0), &flag) && flag == 1);
    CHECK(json_as_bool(json_array_at(root, 1), &flag) && flag == 0);
    CHECK(json_kind_of(json_array_at(root, 2)) == JSON_NULL);
    CHECK(!json_as_bool(json_array_at(root, 2), &flag));
    json_free(root);
}

static void refuses_a_trailing_comma(void) {
    check_refused("[1,\n 2,\n]", 3, 1, "trailing comma");
    check_refused("{\"a\": 1,}", 1, 9, "trailing comma");
}

static void refuses_an_unterminated_string(void) {
    check_refused("[\"abc", 1, 6, "unterminated string");
    check_refused("{\"a\":\n\"b", 2, 3, "unterminated string");
}

static void refuses_a_lone_surrogate(void) {
    check_refused("[\"\\ud83d\"]", 1, 9, "lone high surrogate");
    check_refused("[\"\\ude00\"]", 1, 9, "lone low surrogate");
    check_refused("[\"\\ud83d\\u0041\"]", 1, 15,
                  "high surrogate not followed by a low surrogate");
}

static void refuses_other_malformed_input(void) {
    check_refused("", 1, 1, "unexpected end of input");
    check_refused("[1] x", 1, 5, "unexpected text after the document");
    check_refused("[01]", 1, 3, "leading zero in number");
    check_refused("{a: 1}", 1, 2, "expected a string key");
    check_refused("[\"a\tb\"]", 1, 4, "control character in string");
}

static void refuses_nesting_that_would_overflow_the_stack(void) {
    size_t depth = 1000;
    char *text = malloc(depth + 1);
    json_error error = {0, 0, NULL};
    CHECK(text != NULL);
    memset(text, '[', depth);
    text[depth] = '\0';
    CHECK(json_parse(text, depth, &error) == NULL);
    check_text(error.message, "nesting too deep", "depth message");
    free(text);
}

static char *build_large_document(size_t *length) {
    size_t capacity = 80 * 1024;
    char *text = malloc(capacity);
    size_t used = 0;
    size_t index;
    CHECK(text != NULL);
    used += (size_t)snprintf(text + used, capacity - used, "{\"items\": [");
    for (index = 0; index < 2000; index++) {
        used += (size_t)snprintf(text + used, capacity - used, "%s{\"id\": %u, \"label\": \"entry-%u\"}", index == 0 ? "" : ", ",
                                (unsigned)index, (unsigned)index);
    }
    used += (size_t)snprintf(text + used, capacity - used, "]}");
    *length = used;
    return text;
}

static void reads_a_64_kilobyte_vector(void) {
    size_t length = 0;
    char *text = build_large_document(&length);
    json_value *root;
    const json_value *items;
    int64_t id = 0;
    CHECK(length > 64 * 1024);
    root = json_parse(text, length, NULL);
    CHECK(root != NULL);
    items = json_object_get(root, "items");
    CHECK(json_array_length(items) == 2000);
    CHECK(json_as_int64(json_object_get(json_array_at(items, 1999), "id"), &id) && id == 1999);
    check_text(json_as_string(json_object_get(json_array_at(items, 1234), "label")), "entry-1234", "label");
    json_free(root);
    free(text);
}

static void reads_a_missing_value_as_null(void) {
    CHECK(json_kind_of(NULL) == JSON_NULL);
}

int main(void) {
    reads_objects_and_arrays();
    reads_strings_with_escapes();
    reads_numbers_as_integer_and_double();
    reads_literals();
    refuses_a_trailing_comma();
    refuses_an_unterminated_string();
    refuses_a_lone_surrogate();
    refuses_other_malformed_input();
    refuses_nesting_that_would_overflow_the_stack();
    reads_a_64_kilobyte_vector();
    reads_a_missing_value_as_null();
    return check_summary("json_test");
}
