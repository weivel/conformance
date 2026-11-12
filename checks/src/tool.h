#ifndef CHECKS_TOOL_H
#define CHECKS_TOOL_H

#include <stddef.h>

/**
 * Runs `arguments[0]` with the rest of `arguments`, each reaching the program unchanged whatever spaces or quotes a
 * path holds, and captures its standard output and standard error together. Returns 1 with `output` owned by the
 * caller and `exit_code` set, or 0 with `error` filled when the program could not be started or read.
 */
int tool_run(const char *const *arguments, size_t count, char **output, int *exit_code, char *error,
    size_t error_size);

#endif
