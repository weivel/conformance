#ifndef CONFORMANCE_REPORT_H
#define CONFORMANCE_REPORT_H

#include "runner.h"
#include "text.h"

/**
 * Writes the table a reader judges an implementation by, then the reasons of every vector that did not pass,
 * and returns the process exit code: 0 only when at least one vector ran and every one passed.
 * @implNote Only `passed` is a pass. `awaiting` layer vectors get their own column and are never added to `passed`:
 * a suite that counts a vector it did not run as a pass reports a green it has not earned. An empty run is not a
 * pass either.
 */
int report_write(const verdict *verdicts, size_t count, text_builder *out);

#endif
