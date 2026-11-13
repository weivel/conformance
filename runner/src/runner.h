#ifndef CONFORMANCE_RUNNER_H
#define CONFORMANCE_RUNNER_H

#include "ops/ops.h"
#include "vectors.h"

typedef enum {
    VERDICT_PASSED,
    VERDICT_FAILED,
    VERDICT_ERRORED,
    VERDICT_AWAITING_LAYER
} verdict_status;

#define VERDICT_STATUS_COUNT 4

typedef struct {
    const vector *vector;
    verdict_status status;
    /** One line per reason, each ending in a newline; empty for a pass. Owned by the verdict. */
    char *reasons;
} verdict;

/**
 * Judges one vector with the adapter given for its op.
 * @implNote Only `passed` is a pass. A vector the adapter did not answer, or answered in a document the runner
 * cannot read, is `errored`, never skipped: an absence must not be mistakable for a pass. A core or standard vector
 * is run through its adapter; a layer vector is `awaiting` a layer suite, is not run here, and is never counted as
 * passed.
 */
void judge_vector(const vector *candidate, op_adapter adapter, verdict *judged);

/** Judges every vector of a corpus with the adapters the op table holds. Returns NULL only when out of memory. */
verdict *run_corpus(const corpus *loaded);

void verdicts_free(verdict *verdicts, size_t count);

const char *verdict_label(verdict_status status);

#endif
