#ifndef CONFORMANCE_TEXT_H
#define CONFORMANCE_TEXT_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    int failed;
} text_builder;

void text_init(text_builder *builder);

/** Releases the storage and leaves the builder empty and reusable. */
void text_release(text_builder *builder);

void text_append(text_builder *builder, const char *text);
void text_append_bytes(text_builder *builder, const char *bytes, size_t length);
void text_append_format(text_builder *builder, const char *format, ...);

/**
 * The text so far, always NUL-terminated, valid until the next append or release. After an allocation failure
 * the builder is `failed` and answers an empty string.
 */
const char *text_view(const text_builder *builder);

/** Copies a NUL-terminated string to the heap; NULL on allocation failure. */
char *text_duplicate(const char *text);

#endif
