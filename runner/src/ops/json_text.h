#ifndef CONFORMANCE_OPS_JSON_TEXT_H
#define CONFORMANCE_OPS_JSON_TEXT_H

#include "answer.h"
#include "weivel/json.h"

/**
 * Writes a JSON value back out as text, for a library under test that takes a document rather than a node.
 * Returns 0 for a value the writer cannot reproduce exactly (a number that is not an integer, a string holding a
 * NUL character, nesting deeper than the writer holds), so an adapter answers nothing rather than ask about a
 * document the vector did not give.
 */
int json_text_write(const weivel_json_value *value, answer_writer *out);

#endif
