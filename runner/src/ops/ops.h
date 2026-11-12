#ifndef CONFORMANCE_OPS_H
#define CONFORMANCE_OPS_H

#include "answer.h"
#include "json.h"

/**
 * Asks the engine under test one question and writes its answer.
 *
 * `given` is the vector's `given`, read only and valid for the call. The adapter drives the engine in-process,
 * writes the answer for its op into `answer` as the object RUNNER.md describes for that op, and returns 1.
 * It returns 0 when it cannot answer, and the vector is then errored: an absence is never a pass. An adapter
 * never reads a vector's `then`, because one that does is marking its own paper.
 */
typedef int (*op_adapter)(const json_value *given, answer_writer *answer);

typedef struct {
    const char *op;
    op_adapter adapter;
} op_entry;

/**
 * The entry for an op the kit knows, or NULL for an op it does not. An entry whose adapter is NULL is an op the
 * kit asks about and no adapter yet answers.
 */
const op_entry *ops_find(const char *op);

/** Every op the kit knows, in the order a report lists them. */
const op_entry *ops_all(size_t *count);

#endif
