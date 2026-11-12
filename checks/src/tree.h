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
 * Build output and tool state are not part of a repository's content, so a directory is skipped when its name starts
 * with a dot (except `.weivel`), starts with `build`, or is `siblings`, `node_modules` or `dist`.
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
