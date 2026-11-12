#ifndef CHECKS_CORE_SOURCES_H
#define CHECKS_CORE_SOURCES_H

#include "core_rules.h"

/**
 * Scans every file under `root`'s `src` and `include` for the `forbid-source` names, and for the
 * `source-in-mutex-only` names outside the `mutex-source` file. It sees what an import table cannot: an intrinsic, an
 * inline system call, inline assembly. Returns 0 clean, 1 with every hit printed, 2 when no source was read.
 */
int core_sources_scan(const rule_set *rules, const char *root);

#endif
