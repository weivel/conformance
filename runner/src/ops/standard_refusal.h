#ifndef CONFORMANCE_OPS_STANDARD_REFUSAL_H
#define CONFORMANCE_OPS_STANDARD_REFUSAL_H

#include "answer.h"

#include <weivel/standard.h>

/** The standard's refusal code in lowercase without its prefix, as `not_well_known`; NULL for a code it does not name. */
const char *standard_refusal_name(uint32_t code);

/** Writes `{ code, detail, fix, path }`, or null for an empty refusal. */
void standard_answer_refusal(answer_writer *answer, const weivel_standard_refusal *refusal);

#endif
