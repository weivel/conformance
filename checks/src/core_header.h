#ifndef CHECKS_CORE_HEADER_H
#define CHECKS_CORE_HEADER_H

#include <stddef.h>

#include "core_rules.h"

/**
 * Reads each header a `public-header` rule names under `root` and adds to `offenders` every function pointer type it
 * declares: a pointer declarator such as `(*name)(`, `(* const name)(`, `(*)(` or `(CALL *name)(`, and a typedef of a
 * function type such as `typedef int name(int);`. Comments and string literals are not read, and a macro's text is.
 * The core never calls out, so its public interface hands it nothing it could call. Returns 1, or 0 with the cause
 * printed when a named header cannot be read, since its declarations were then not checked.
 */
int core_header_scan(const rule_set *rules, const char *root, size_t *offenders);

#endif
