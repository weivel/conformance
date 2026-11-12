#include "json.h"

#include <stdlib.h>
#include <string.h>

#define MAXIMUM_DEPTH 256

typedef struct {
    char *key;
    json_value *value;
} json_member;

struct json_value {
    json_kind kind;
    int boolean;
    int is_integer;
    int64_t integer;
    double real;
    char *text;
    size_t text_length;
    size_t count;
    size_t capacity;
    json_member *members;
};

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} text_buffer;

typedef struct {
    const char *text;
    size_t length;
    size_t position;
    int depth;
    int failed;
    json_error failure;
} parser;

static json_value *parse_value(parser *state);

static json_value *fail(parser *state, const char *message) {
    int line = 1;
    int column = 1;
    size_t index;
    state->failed = 1;
    state->failure.message = message;
    for (index = 0; index < state->position && index < state->length; index++) {
        if (state->text[index] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    state->failure.line = line;
    state->failure.column = column;
    return NULL;
}

static int has_more(const parser *state) {
    return state->position < state->length;
}

static char peek(const parser *state) {
    return has_more(state) ? state->text[state->position] : '\0';
}

static void skip_whitespace(parser *state) {
    while (has_more(state)) {
        char current = peek(state);
        if (current != ' ' && current != '\t' && current != '\n' && current != '\r') {
            return;
        }
        state->position++;
    }
}

static json_value *new_value(json_kind kind) {
    json_value *value = calloc(1, sizeof(*value));
    if (value != NULL) {
        value->kind = kind;
    }
    return value;
}

static int buffer_append(text_buffer *buffer, const char *bytes, size_t count) {
    if (buffer->length + count + 1 > buffer->capacity) {
        size_t wanted = buffer->capacity == 0 ? 32 : buffer->capacity;
        char *grown;
        while (wanted < buffer->length + count + 1) {
            wanted *= 2;
        }
        grown = realloc(buffer->data, wanted);
        if (grown == NULL) {
            return 0;
        }
        buffer->data = grown;
        buffer->capacity = wanted;
    }
    memcpy(buffer->data + buffer->length, bytes, count);
    buffer->length += count;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int append_code_point(text_buffer *buffer, unsigned long code_point) {
    char bytes[4];
    size_t count;
    if (code_point < 0x80) {
        bytes[0] = (char)code_point;
        count = 1;
    } else if (code_point < 0x800) {
        bytes[0] = (char)(0xC0 | (code_point >> 6));
        bytes[1] = (char)(0x80 | (code_point & 0x3F));
        count = 2;
    } else if (code_point < 0x10000) {
        bytes[0] = (char)(0xE0 | (code_point >> 12));
        bytes[1] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        bytes[2] = (char)(0x80 | (code_point & 0x3F));
        count = 3;
    } else {
        bytes[0] = (char)(0xF0 | (code_point >> 18));
        bytes[1] = (char)(0x80 | ((code_point >> 12) & 0x3F));
        bytes[2] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        bytes[3] = (char)(0x80 | (code_point & 0x3F));
        count = 4;
    }
    return buffer_append(buffer, bytes, count);
}

static int hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

static int read_hex4(parser *state, unsigned long *out) {
    unsigned long accumulated = 0;
    int index;
    for (index = 0; index < 4; index++) {
        int digit = has_more(state) ? hex_digit(peek(state)) : -1;
        if (digit < 0) {
            fail(state, "expected four hex digits after \\u");
            return 0;
        }
        accumulated = accumulated * 16 + (unsigned long)digit;
        state->position++;
    }
    *out = accumulated;
    return 1;
}

static int is_high_surrogate(unsigned long unit) {
    return unit >= 0xD800 && unit <= 0xDBFF;
}

static int is_low_surrogate(unsigned long unit) {
    return unit >= 0xDC00 && unit <= 0xDFFF;
}

static int read_unicode_escape(parser *state, text_buffer *buffer) {
    unsigned long unit;
    if (!read_hex4(state, &unit)) {
        return 0;
    }
    if (is_low_surrogate(unit)) {
        fail(state, "lone low surrogate");
        return 0;
    }
    if (is_high_surrogate(unit)) {
        unsigned long low;
        if (state->position + 1 >= state->length || state->text[state->position] != '\\' ||
            state->text[state->position + 1] != 'u') {
            fail(state, "lone high surrogate");
            return 0;
        }
        state->position += 2;
        if (!read_hex4(state, &low)) {
            return 0;
        }
        if (!is_low_surrogate(low)) {
            fail(state, "high surrogate not followed by a low surrogate");
            return 0;
        }
        unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
    }
    if (!append_code_point(buffer, unit)) {
        fail(state, "out of memory");
        return 0;
    }
    return 1;
}

static int read_escape(parser *state, text_buffer *buffer) {
    char escaped = peek(state);
    const char *simple = NULL;
    switch (escaped) {
        case '"': simple = "\""; break;
        case '\\': simple = "\\"; break;
        case '/': simple = "/"; break;
        case 'b': simple = "\b"; break;
        case 'f': simple = "\f"; break;
        case 'n': simple = "\n"; break;
        case 'r': simple = "\r"; break;
        case 't': simple = "\t"; break;
        default: break;
    }
    if (escaped == 'u') {
        state->position++;
        return read_unicode_escape(state, buffer);
    }
    if (simple == NULL) {
        fail(state, "invalid escape");
        return 0;
    }
    state->position++;
    if (!buffer_append(buffer, simple, 1)) {
        fail(state, "out of memory");
        return 0;
    }
    return 1;
}

static int read_string_body(parser *state, text_buffer *buffer) {
    while (has_more(state)) {
        char current = peek(state);
        if (current == '"') {
            state->position++;
            return 1;
        }
        if ((unsigned char)current < 0x20) {
            fail(state, "control character in string");
            return 0;
        }
        state->position++;
        if (current == '\\') {
            if (!has_more(state)) {
                break;
            }
            if (!read_escape(state, buffer)) {
                return 0;
            }
        } else if (!buffer_append(buffer, &current, 1)) {
            fail(state, "out of memory");
            return 0;
        }
    }
    fail(state, "unterminated string");
    return 0;
}

static int read_string(parser *state, text_buffer *buffer) {
    state->position++;
    if (!buffer_append(buffer, "", 0)) {
        fail(state, "out of memory");
        return 0;
    }
    return read_string_body(state, buffer);
}

static json_value *parse_string(parser *state) {
    text_buffer buffer = {NULL, 0, 0};
    json_value *value;
    if (!read_string(state, &buffer)) {
        free(buffer.data);
        return NULL;
    }
    value = new_value(JSON_STRING);
    if (value == NULL) {
        free(buffer.data);
        return fail(state, "out of memory");
    }
    value->text = buffer.data;
    value->text_length = buffer.length;
    return value;
}

static int matches_literal(const parser *state, const char *literal) {
    size_t length = strlen(literal);
    return state->position + length <= state->length && memcmp(state->text + state->position, literal, length) == 0;
}

static json_value *parse_literal(parser *state, const char *literal, json_kind kind, int boolean) {
    json_value *value;
    if (!matches_literal(state, literal)) {
        return fail(state, "unexpected character");
    }
    value = new_value(kind);
    if (value == NULL) {
        return fail(state, "out of memory");
    }
    state->position += strlen(literal);
    value->boolean = boolean;
    return value;
}

static int is_digit(char character) {
    return character >= '0' && character <= '9';
}

static size_t scan_digits(const parser *state, size_t from) {
    size_t end = from;
    while (end < state->length && is_digit(state->text[end])) {
        end++;
    }
    return end;
}

static int scan_number_end(parser *state, size_t *end, int *has_fraction_or_exponent) {
    size_t cursor = state->position;
    size_t digits_end;
    if (cursor < state->length && state->text[cursor] == '-') {
        cursor++;
    }
    digits_end = scan_digits(state, cursor);
    if (digits_end == cursor) {
        state->position = cursor;
        fail(state, "expected a digit");
        return 0;
    }
    if (state->text[cursor] == '0' && digits_end - cursor > 1) {
        state->position = cursor + 1;
        fail(state, "leading zero in number");
        return 0;
    }
    cursor = digits_end;
    *has_fraction_or_exponent = 0;
    if (cursor < state->length && state->text[cursor] == '.') {
        digits_end = scan_digits(state, cursor + 1);
        if (digits_end == cursor + 1) {
            state->position = cursor + 1;
            fail(state, "expected a digit after the decimal point");
            return 0;
        }
        cursor = digits_end;
        *has_fraction_or_exponent = 1;
    }
    if (cursor < state->length && (state->text[cursor] == 'e' || state->text[cursor] == 'E')) {
        size_t exponent_start = cursor + 1;
        if (exponent_start < state->length && (state->text[exponent_start] == '+' || state->text[exponent_start] == '-')) {
            exponent_start++;
        }
        digits_end = scan_digits(state, exponent_start);
        if (digits_end == exponent_start) {
            state->position = exponent_start;
            fail(state, "expected a digit in the exponent");
            return 0;
        }
        cursor = digits_end;
        *has_fraction_or_exponent = 1;
    }
    *end = cursor;
    return 1;
}

static int accumulate_integer(const char *digits, int negative, int64_t *out) {
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    uint64_t accumulated = 0;
    for (; *digits != '\0'; digits++) {
        uint64_t digit = (uint64_t)(*digits - '0');
        if (accumulated > (limit - digit) / 10) {
            return 0;
        }
        accumulated = accumulated * 10 + digit;
    }
    *out = negative ? (int64_t)(0 - accumulated) : (int64_t)accumulated;
    return 1;
}

static json_value *parse_number(parser *state) {
    size_t end;
    int inexact;
    size_t length;
    char *copy;
    json_value *value;
    if (!scan_number_end(state, &end, &inexact)) {
        return NULL;
    }
    length = end - state->position;
    copy = malloc(length + 1);
    value = new_value(JSON_NUMBER);
    if (copy == NULL || value == NULL) {
        free(copy);
        free(value);
        return fail(state, "out of memory");
    }
    memcpy(copy, state->text + state->position, length);
    copy[length] = '\0';
    value->real = strtod(copy, NULL);
    if (!inexact) {
        int negative = copy[0] == '-';
        value->is_integer = accumulate_integer(copy + negative, negative, &value->integer);
    }
    free(copy);
    state->position = end;
    return value;
}

static int append_member(json_value *container, char *key, json_value *member) {
    if (container->count == container->capacity) {
        size_t wanted = container->capacity == 0 ? 4 : container->capacity * 2;
        json_member *grown = realloc(container->members, wanted * sizeof(*grown));
        if (grown == NULL) {
            return 0;
        }
        container->members = grown;
        container->capacity = wanted;
    }
    container->members[container->count].key = key;
    container->members[container->count].value = member;
    container->count++;
    return 1;
}

static int enter_container(parser *state) {
    if (state->depth >= MAXIMUM_DEPTH) {
        fail(state, "nesting too deep");
        return 0;
    }
    state->depth++;
    return 1;
}

static int read_object_key(parser *state, char **key) {
    text_buffer buffer = {NULL, 0, 0};
    if (peek(state) != '"') {
        fail(state, "expected a string key");
        return 0;
    }
    if (!read_string(state, &buffer)) {
        free(buffer.data);
        return 0;
    }
    *key = buffer.data;
    return 1;
}

static int read_object_member(parser *state, json_value *object) {
    char *key;
    json_value *member;
    if (!read_object_key(state, &key)) {
        return 0;
    }
    skip_whitespace(state);
    if (peek(state) != ':') {
        free(key);
        fail(state, "expected ':' after key");
        return 0;
    }
    state->position++;
    member = parse_value(state);
    if (member == NULL) {
        free(key);
        return 0;
    }
    if (!append_member(object, key, member)) {
        free(key);
        json_free(member);
        fail(state, "out of memory");
        return 0;
    }
    return 1;
}

static int read_array_element(parser *state, json_value *array) {
    json_value *element = parse_value(state);
    if (element == NULL) {
        return 0;
    }
    if (!append_member(array, NULL, element)) {
        json_free(element);
        fail(state, "out of memory");
        return 0;
    }
    return 1;
}

static int read_container_items(parser *state, json_value *container, char closing) {
    skip_whitespace(state);
    if (peek(state) == closing) {
        state->position++;
        return 1;
    }
    for (;;) {
        int read = container->kind == JSON_OBJECT ? read_object_member(state, container)
                                                  : read_array_element(state, container);
        if (!read) {
            return 0;
        }
        skip_whitespace(state);
        if (peek(state) == closing) {
            state->position++;
            return 1;
        }
        if (peek(state) != ',') {
            fail(state, container->kind == JSON_OBJECT ? "expected ',' or '}'" : "expected ',' or ']'");
            return 0;
        }
        state->position++;
        skip_whitespace(state);
        if (peek(state) == closing) {
            fail(state, "trailing comma");
            return 0;
        }
    }
}

static json_value *parse_container(parser *state, json_kind kind, char closing) {
    json_value *container;
    if (!enter_container(state)) {
        return NULL;
    }
    container = new_value(kind);
    if (container == NULL) {
        return fail(state, "out of memory");
    }
    state->position++;
    if (!read_container_items(state, container, closing)) {
        json_free(container);
        return NULL;
    }
    state->depth--;
    return container;
}

static json_value *parse_value(parser *state) {
    skip_whitespace(state);
    if (!has_more(state)) {
        return fail(state, "unexpected end of input");
    }
    switch (peek(state)) {
        case '{': return parse_container(state, JSON_OBJECT, '}');
        case '[': return parse_container(state, JSON_ARRAY, ']');
        case '"': return parse_string(state);
        case 't': return parse_literal(state, "true", JSON_BOOL, 1);
        case 'f': return parse_literal(state, "false", JSON_BOOL, 0);
        case 'n': return parse_literal(state, "null", JSON_NULL, 0);
        default: return parse_number(state);
    }
}

json_value *json_parse(const char *text, size_t length, json_error *error) {
    parser state = {NULL, 0, 0, 0, 0, {0, 0, NULL}};
    json_value *root;
    state.text = text;
    state.length = length;
    root = parse_value(&state);
    if (root != NULL) {
        skip_whitespace(&state);
        if (has_more(&state)) {
            json_free(root);
            root = fail(&state, "unexpected text after the document");
        }
    }
    if (root == NULL && error != NULL) {
        *error = state.failure;
    }
    return root;
}

void json_free(json_value *value) {
    size_t index;
    if (value == NULL) {
        return;
    }
    for (index = 0; index < value->count; index++) {
        free(value->members[index].key);
        json_free(value->members[index].value);
    }
    free(value->members);
    free(value->text);
    free(value);
}

json_kind json_kind_of(const json_value *value) {
    return value == NULL ? JSON_NULL : value->kind;
}

const json_value *json_object_get(const json_value *value, const char *key) {
    size_t index;
    if (value == NULL || value->kind != JSON_OBJECT) {
        return NULL;
    }
    for (index = 0; index < value->count; index++) {
        if (strcmp(value->members[index].key, key) == 0) {
            return value->members[index].value;
        }
    }
    return NULL;
}

size_t json_object_size(const json_value *value) {
    return value != NULL && value->kind == JSON_OBJECT ? value->count : 0;
}

const char *json_object_key_at(const json_value *value, size_t index) {
    return index < json_object_size(value) ? value->members[index].key : NULL;
}

const json_value *json_object_value_at(const json_value *value, size_t index) {
    return index < json_object_size(value) ? value->members[index].value : NULL;
}

size_t json_array_length(const json_value *value) {
    return value != NULL && value->kind == JSON_ARRAY ? value->count : 0;
}

const json_value *json_array_at(const json_value *value, size_t index) {
    return index < json_array_length(value) ? value->members[index].value : NULL;
}

const char *json_as_string(const json_value *value) {
    return value != NULL && value->kind == JSON_STRING ? value->text : NULL;
}

size_t json_string_length(const json_value *value) {
    return value != NULL && value->kind == JSON_STRING ? value->text_length : 0;
}

int json_as_bool(const json_value *value, int *out) {
    if (value == NULL || value->kind != JSON_BOOL) {
        return 0;
    }
    *out = value->boolean;
    return 1;
}

int json_as_int64(const json_value *value, int64_t *out) {
    if (value == NULL || value->kind != JSON_NUMBER || !value->is_integer) {
        return 0;
    }
    *out = value->integer;
    return 1;
}

int json_as_double(const json_value *value, double *out) {
    if (value == NULL || value->kind != JSON_NUMBER) {
        return 0;
    }
    *out = value->real;
    return 1;
}
