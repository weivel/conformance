#ifndef CONFORMANCE_OPS_STANDARD_REFUSAL_H
#define CONFORMANCE_OPS_STANDARD_REFUSAL_H

#include "answer.h"

#include <weivel/standard.h>

/** The standard's refusal code in lowercase without its prefix, as `not_well_known`; NULL for a code it does not name. */
const char *standard_refusal_name(uint32_t code);

/** One past the highest code the kit names: every code below it has a name. */
uint32_t standard_refusal_named_codes(void);

/**
 * 1 for an empty refusal or one carrying both a `detail` and a `fix`, else 0: a standard that refuses without saying
 * what happened or what would fix it has not answered, so it cannot pass a vector by omitting them.
 */
int standard_refusal_is_answerable(const weivel_standard_refusal *refusal);

/** Writes `{ code, detail, fix, path }`, or null for an empty refusal; 0, writing nothing, for one not answerable. */
int standard_answer_refusal(answer_writer *answer, const weivel_standard_refusal *refusal);

#endif
