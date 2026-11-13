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

/**
 * 1 when `handle` names a want still waiting or a watch still hearing, as weivel_handle_state answers it, else 0.
 * @implNote Only waiting and watching are read. The core forgets how a want or watch ended once its handle's slot
 * is reused, so "settled" or "withdrawn" read here would be a guess; how a want ended is read from its settlement
 * event, and anything but waiting or watching, a refused query included, means it no longer is.
 */
int session_still_live(core_session *session, weivel_handle handle);

/** The refusal code as RUNNER.md spells it: the WEIVEL_REFUSAL_ name in lowercase, NULL for one the kit does not know. */
const char *session_refusal_name(uint32_t code);

/** A refusal as the core answers it, with what it is about (NULL when nothing) and whether its fix is non-empty. */
typedef struct {
    uint32_t code;
    const char *detail;
    const char *fix;
    const char *subject;
    uint32_t has_fix;
} session_refusal;

/** Writes `{ code, detail, fix, subject, hasFix }`, or null for code NONE. */
void session_answer_refusal(answer_writer *answer, const session_refusal *refusal);

#endif
