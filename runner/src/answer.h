#ifndef CONFORMANCE_ANSWER_H
#define CONFORMANCE_ANSWER_H

#include "text.h"

#include <stdint.h>

#define ANSWER_MAXIMUM_DEPTH 32

/**
 * Writes the answer an adapter gives for one vector, as a JSON document the matcher reads back.
 * @implNote The matcher compares an answer against a vector's `then` through the one reader the kit has, so
 * an answer is text first. The text stays inside the runner: it never reaches the core or a library under test.
 * Calls must nest correctly; a call out of order marks the writer `broken`, and the runner treats a broken
 * answer as an error and not as a pass.
 */
typedef struct {
    text_builder text;
    int has_items[ANSWER_MAXIMUM_DEPTH];
    size_t depth;
    int after_key;
    int broken;
} answer_writer;

void answer_init(answer_writer *writer);
void answer_release(answer_writer *writer);

void answer_begin_object(answer_writer *writer);
void answer_end_object(answer_writer *writer);
void answer_begin_array(answer_writer *writer);
void answer_end_array(answer_writer *writer);

/** Names the next member of the open object. */
void answer_key(answer_writer *writer, const char *key);

void answer_string(answer_writer *writer, const char *value);
void answer_int(answer_writer *writer, int64_t value);
void answer_bool(answer_writer *writer, int value);
void answer_null(answer_writer *writer);

/** The finished document; valid until the next write or release. Empty when nothing was written. */
const char *answer_text(const answer_writer *writer);

/** 1 when the document is complete: something was written, every container closed, nothing out of order. */
int answer_is_complete(const answer_writer *writer);

#endif
