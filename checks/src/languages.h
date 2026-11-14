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
    char *source_base;
    char *test_root;
    char *header_root;
    int known;
} language_rules;

/**
 * Reads the lines of `path` that begin with `language`, or every line when `language` is NULL. Each line is
 * `<language> <kind> <value>`, where kind is `extension`, `name`, `pattern` (one `*` in a file name, standing for any
 * characters, and for at least one when the pattern begins with it), `prefix` (a path prefix), `source-root`,
 * `source-base`, `test-root` or `header-root`. A source base is a directory holding the source root, under which parity
 * refuses every file outside the source root, so a language whose modules sit below a package root states where its
 * sources may lie as well as where its modules are. A header root is the directory a language publishes its interface
 * in: parity reads every file under it, skipping no directory by name, and refuses one that lacks an extension of the
 * language or belongs to no listed module. It asks no header of any module, and reads nothing beside the header root.
 * Returns 1, or 0 with `error` filled for an unreadable file or a malformed line. `known` is 0 when no line named
 * the language.
 */
int language_rules_load(const char *path, const char *language, language_rules *rules, char *error,
                        size_t error_size);

/**
 * Reads the lines of `path` that begin with any language but `language`, as language_rules_load reads them. `known`
 * is 0 when no other language has a line.
 */
int language_rules_load_others(const char *path, const char *language, language_rules *rules, char *error,
                               size_t error_size);

void language_rules_free(language_rules *rules);

#endif
