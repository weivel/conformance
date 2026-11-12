#ifndef CONFORMANCE_MATCH_H
#define CONFORMANCE_MATCH_H

#include "json.h"
#include "text.h"

#include <stddef.h>

/**
 * Compares one answer against one vector's `then`, structurally and never by wording. Each key of `then` is
 * compared with the same key of `result`; a key absent from `then` asserts nothing.
 *
 * | in `then`                  | holds when                                                            |
 * | `null`                     | the key is absent or null in the result                               |
 * | `{"code": "x"}`            | the result's value is a refusal carrying the code `x`                 |
 * | `{"before": [["a","b"]]}`  | the result is an array in which `a` precedes `b`, pairwise            |
 * | `{"contains": [x]}`        | the result is an array containing each element                        |
 * | `{"equals": x}`            | the result equals `x` exactly                                         |
 * | `{"absent": true}`         | the key is absent or null                                             |
 * | anything else              | deep equality                                                         |
 *
 * Appends one line to `reasons` for each assertion that does not hold, and returns how many did not.
 * `result` is the whole answer object, and is never NULL: a vector no adapter answered is errored by the
 * runner and never reaches here.
 */
size_t match_then(const json_value *then, const json_value *result, text_builder *reasons);

/** Writes a value as compact JSON, so a reason can quote what an adapter answered. */
void match_render(text_builder *out, const json_value *value);

#endif
