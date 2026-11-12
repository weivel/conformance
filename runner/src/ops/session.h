#ifndef CONFORMANCE_OPS_SESSION_H
#define CONFORMANCE_OPS_SESSION_H

#include "answer.h"

#include <weivel/core.h>

/** Reads one event while the session drains; the event is released after it returns. */
typedef void (*event_reader)(const weivel_event *event, void *context);

/**
 * One core an adapter drives as a host would, draining the queue after each call it makes.
 * @implNote The reader may call the core, as a host answering a capacity question does; what that call queues is
 * drained by the same loop, so the reader never drains itself.
 */
typedef struct {
    weivel_core *core;
    event_reader reader;
    void *context;
    int failed;
} core_session;

/** Creates the core and drains what creating it queued. 0 when the core could not be created. */
int session_open(core_session *session, const weivel_core_options *options, event_reader reader, void *context);

/** Drains every queued event through the reader. */
void session_drain(core_session *session);

void session_close(core_session *session);

/** The refusal code as RUNNER.md spells it: the WEIVEL_REFUSAL_ name in lowercase, NULL for one the kit does not know. */
const char *session_refusal_name(uint32_t code);

/** Writes `{ code, detail, fix }`, or null for code NONE. */
void session_answer_refusal(answer_writer *answer, uint32_t code, const char *detail, const char *fix);

#endif
