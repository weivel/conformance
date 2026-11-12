#ifndef CHECKS_LANGUAGES_H
#define CHECKS_LANGUAGES_H

#include <stddef.h>

#include "tree.h"

typedef struct {
    text_list extensions;
    text_list names;
    text_list patterns;
    text_list prefixes;
    char *source_root;
    int known;
} language_rules;

/**
 * Reads the lines of `path` that begin with `language`, or every line when `language` is NULL. Each line is `<language> <kind> <value>`, where kind is
 * `extension`, `name`, `pattern` (one `*` in a file name), `prefix` (a path prefix) or `source-root`.
 * Returns 1, or 0 with `error` filled for an unreadable file or a malformed line. `known` is 0 when no line named
 * the language.
 */
int language_rules_load(const char *path, const char *language, language_rules *rules, char *error,
                        size_t error_size);

void language_rules_free(language_rules *rules);

#endif
