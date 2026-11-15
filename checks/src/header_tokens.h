#ifndef CHECKS_HEADER_TOKENS_H
#define CHECKS_HEADER_TOKENS_H

#include <stddef.h>

/** One preprocessing token of a header, pointing into the preprocessed text, with the header line it came from. */
typedef struct {
    const char *text;
    size_t length;
    size_t line;
} header_token;

typedef struct {
    header_token *items;
    size_t count;
    char *preprocessed;
} header_tokens;

/**
 * Preprocesses the header at `path` with the C compiler the kit was built with, its `#include` lines blanked, and
 * tokenizes what came from the header itself, each token carrying its line in the header. String and character
 * literals are dropped. Returns 1, or 0 with the cause in `error`.
 */
int header_tokens_read(const char *path, header_tokens *tokens, char *error, size_t error_size);

void header_tokens_free(header_tokens *tokens);

#endif
