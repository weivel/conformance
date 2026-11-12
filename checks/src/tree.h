#ifndef CHECKS_TREE_H
#define CHECKS_TREE_H

#include <stddef.h>

typedef struct {
    char **items;
    size_t count;
} text_list;

int text_list_add(text_list *list, const char *text);
void text_list_free(text_list *list);

/** Joins a directory and a name with a forward slash. The result is owned by the caller. */
char *tree_join(const char *directory, const char *name);

/** The path without trailing separators and with forward slashes. The result is owned by the caller. */
char *tree_normalize_root(const char *path);

/**
 * Lists every file under `root` at any depth as a path relative to it with forward slashes, ordered by path.
 * Version control, hosting configuration and build output are not part of a repository's content, so a directory
 * directly under `root` is skipped when it is `.git`, the hosting-service directory next to it, `build`, `build-`
 * followed by anything, `siblings` or `node_modules`. Deeper down nothing is skipped by name, so `src/builder` or a
 * nested `node_modules` is read like any other directory. On POSIX a symbolic link is neither listed nor followed, and
 * only regular files are listed.
 * Returns 1, or 0 with `error` filled.
 */
int tree_list_files(const char *root, text_list *files, char *error, size_t error_size);

/** Lists the immediate subdirectories of `directory`, by name, ordered. Returns 1, or 0 with `error` filled. */
int tree_list_directories(const char *directory, text_list *names, char *error, size_t error_size);

/** Reads the trimmed, non-empty lines of a text file. Returns 1, or 0 with `error` filled. */
int tree_read_lines(const char *path, text_list *lines, char *error, size_t error_size);

/** Reads a whole file. Returns 1 and a NUL-terminated buffer owned by the caller, or 0. */
int tree_read_file(const char *path, char **data, size_t *size);

#endif
