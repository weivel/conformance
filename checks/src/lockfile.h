#ifndef CHECKS_LOCKFILE_H
#define CHECKS_LOCKFILE_H

#include <stddef.h>

#include "tree.h"

/** Whether `relative` names an npm lockfile, which the names check reads structurally rather than as text. */
int lockfile_is_lockfile(const char *relative);

/**
 * Reports each word of `words` (already case-folded) found in the parts of an npm lockfile that can carry a
 * dependency's name or origin: the top-level `name`, every key of `packages`, and in each entry its `name`, `version`
 * and `resolved` and the keys and specs of its `dependencies`, `devDependencies`, `optionalDependencies`,
 * `peerDependencies`, `peerDependenciesMeta` and `requires`, recursing into a legacy nested `dependencies` tree.
 * Every other member, `funding`, `license` and `integrity` among them, names no dependency and is not read.
 *
 * @return the number of offenders, counting a text that is not a JSON object as one, so a lockfile the check
 * cannot read is never passed unread.
 */
int lockfile_report_words(const char *relative, const char *data, size_t size, const text_list *words);

#endif
