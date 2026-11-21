#include "run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int stub_record_begin(const char *record_path) {
    FILE *file = fopen(record_path, "wb");
    if (file == NULL) {
        return 0;
    }
    return fclose(file) == 0;
}

void stub_record_line(const char *record_path, const char *line) {
    FILE *file = fopen(record_path, "ab");
    if (file == NULL) {
        return;
    }
    fputs(line, file);
    fputc('\n', file);
    fclose(file);
}

static void add_bytes(stub_line *line, const char *bytes, size_t length) {
    if (line->text == NULL && line->capacity != 0) {
        return;
    }
    if (line->length + length + 1 > line->capacity) {
        size_t capacity = line->capacity == 0 ? 64 : line->capacity;
        char *grown;
        while (capacity < line->length + length + 1) {
            capacity *= 2;
        }
        grown = realloc(line->text, capacity);
        if (grown == NULL) {
            free(line->text);
            line->text = NULL;
            line->capacity = 1;
            return;
        }
        line->text = grown;
        line->capacity = capacity;
    }
    memcpy(line->text + line->length, bytes, length);
    line->length += length;
    line->text[line->length] = '\0';
}

void stub_line_add(stub_line *line, const char *text) {
    add_bytes(line, text, strlen(text));
}

void stub_line_add_quoted(stub_line *line, const char *text, size_t length) {
    size_t at;
    add_bytes(line, "\"", 1);
    for (at = 0; at < length; at++) {
        unsigned char byte = (unsigned char)text[at];
        char escaped[8];
        if (byte == '"' || byte == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)byte;
            add_bytes(line, escaped, 2);
        } else if (byte == '\n') {
            add_bytes(line, "\\n", 2);
        } else if (byte == '\r') {
            add_bytes(line, "\\r", 2);
        } else if (byte == '\t') {
            add_bytes(line, "\\t", 2);
        } else if (byte < 0x20) {
            snprintf(escaped, sizeof escaped, "\\u%04x", (unsigned)byte);
            add_bytes(line, escaped, 6);
        } else {
            add_bytes(line, (const char *)&text[at], 1);
        }
    }
    add_bytes(line, "\"", 1);
}

/** The shortest `%g` form that reads back as the same double, as JSON writers in most languages answer it. */
static void add_number(stub_line *line, double number) {
    char text[40];
    int precision;
    for (precision = 1; precision <= 17; precision++) {
        snprintf(text, sizeof text, "%.*g", precision, number);
        if (strtod(text, NULL) == number) {
            break;
        }
    }
    stub_line_add(line, text);
}

static void add_value_at(stub_line *line, const weivel_standard_value *value, int depth) {
    size_t at;
    if (depth > WEIVEL_STANDARD_VALUE_DEPTH_LIMIT) {
        stub_line_add(line, "<too-deep>");
        return;
    }
    switch (value->kind) {
    case WEIVEL_STANDARD_VALUE_NULL:
        stub_line_add(line, "null");
        break;
    case WEIVEL_STANDARD_VALUE_BOOLEAN:
        stub_line_add(line, value->boolean ? "true" : "false");
        break;
    case WEIVEL_STANDARD_VALUE_NUMBER:
        add_number(line, value->number);
        break;
    case WEIVEL_STANDARD_VALUE_TEXT:
        stub_line_add_quoted(line, value->text == NULL ? "" : value->text, value->text == NULL ? 0 : value->length);
        break;
    case WEIVEL_STANDARD_VALUE_LIST:
        stub_line_add(line, "[");
        for (at = 0; at < value->count; at++) {
            stub_line_add(line, at == 0 ? "" : ",");
            add_value_at(line, &value->items[at], depth + 1);
        }
        stub_line_add(line, "]");
        break;
    case WEIVEL_STANDARD_VALUE_RECORD:
        stub_line_add(line, "{");
        for (at = 0; at < value->count; at++) {
            stub_line_add(line, at == 0 ? "" : ",");
            stub_line_add_quoted(line, value->entries[at].key, value->entries[at].key_length);
            stub_line_add(line, ":");
            add_value_at(line, &value->entries[at].value, depth + 1);
        }
        stub_line_add(line, "}");
        break;
    case WEIVEL_STANDARD_VALUE_NONE:
        stub_line_add(line, "none");
        break;
    default:
        stub_line_add(line, "<unknown-kind>");
        break;
    }
}

void stub_line_add_value(stub_line *line, const weivel_standard_value *value) {
    add_value_at(line, value, 0);
}

void stub_line_add_setting(stub_line *line, const weivel_standard_setting_value *value) {
    switch (value->kind) {
    case WEIVEL_STANDARD_SETTING_TEXT:
        stub_line_add_quoted(line, value->text, value->text_length);
        break;
    case WEIVEL_STANDARD_SETTING_NUMBER:
        add_number(line, value->number);
        break;
    case WEIVEL_STANDARD_SETTING_BOOLEAN:
        stub_line_add(line, value->boolean ? "true" : "false");
        break;
    default:
        stub_line_add(line, "none");
        break;
    }
}

void stub_line_free(stub_line *line) {
    free(line->text);
    line->text = NULL;
    line->length = 0;
    line->capacity = 0;
}
