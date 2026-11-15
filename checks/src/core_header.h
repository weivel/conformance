#ifndef CHECKS_CORE_HEADER_H
#define CHECKS_CORE_HEADER_H

#include <stddef.h>

#include "core_rules.h"

/**
 * Reads each header a `public-header` rule names under `root`, after the C preprocessor the kit was built with has
 * expanded its own macros, and adds to `offenders` every line that declares a function pointer type: a pointer
 * declarator followed by a parameter list, whatever qualifiers, attributes and calling-convention words it carries
 * (`(**name)(`, `(*name[4])(`, `(*name(void))(`, `(CALL *name)(`, `(*)(`), a parameter of function type, and a
 * typedef of a function type. The core never calls out, so its public interface hands it nothing it could call.
 * Returns 1, or 0 with the cause printed when a named header cannot be read or preprocessed.
 *
 * @implNote The header's `#include` lines are blanked before it is preprocessed, so only its own text is read and
 * no system header or toolchain search path is needed; a macro defined only in a system header stays unexpanded.
 */
int core_header_scan(const rule_set *rules, const char *root, size_t *offenders);

#endif
