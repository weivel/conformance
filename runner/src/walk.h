#ifndef CONFORMANCE_WALK_H
#define CONFORMANCE_WALK_H

#include <stddef.h>

typedef struct {
    char **paths;
    size_t count;
} path_list;

/**
 * Lists every `.json` file under a directory, at any depth, as `directory/relative/path` with forward slashes,
 * ordered by path so a report does not depend on the file system's ordering. Returns 1, or 0 with `error`
 * filled when a directory cannot be read.
 */
int walk_json_files(const char *directory, path_list *found, char *error, size_t error_size);

/** The length of a directory name without its trailing separators, so a path in a list can be cut to its relative part. */
size_t walk_root_length(const char *directory);

void path_list_free(path_list *list);

#endif
