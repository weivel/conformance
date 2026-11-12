#include "text.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void text_init(text_builder *builder) {
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
    builder->failed = 0;
}

void text_release(text_builder *builder) {
    free(builder->data);
    text_init(builder);
}

static int reserve(text_builder *builder, size_t extra) {
    size_t needed = builder->length + extra + 1;
    size_t capacity;
    char *grown;
    if (builder->failed) {
        return 0;
    }
    if (needed <= builder->capacity) {
        return 1;
    }
    capacity = builder->capacity == 0 ? 64 : builder->capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    grown = realloc(builder->data, capacity);
    if (grown == NULL) {
        builder->failed = 1;
        return 0;
    }
    builder->data = grown;
    builder->capacity = capacity;
    return 1;
}

void text_append_bytes(text_builder *builder, const char *bytes, size_t length) {
    if (!reserve(builder, length)) {
        return;
    }
    memcpy(builder->data + builder->length, bytes, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
}

void text_append(text_builder *builder, const char *text) {
    text_append_bytes(builder, text, strlen(text));
}

void text_append_format(text_builder *builder, const char *format, ...) {
    va_list arguments;
    va_list measuring;
    int written;
    va_start(arguments, format);
    va_copy(measuring, arguments);
    written = vsnprintf(NULL, 0, format, measuring);
    va_end(measuring);
    if (written >= 0 && reserve(builder, (size_t)written)) {
        vsnprintf(builder->data + builder->length, (size_t)written + 1, format, arguments);
        builder->length += (size_t)written;
    }
    va_end(arguments);
}

const char *text_view(const text_builder *builder) {
    return builder->data == NULL || builder->failed ? "" : builder->data;
}

char *text_duplicate(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}
