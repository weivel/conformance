#ifndef CONFORMANCE_OPS_HEARING_H
#define CONFORMANCE_OPS_HEARING_H

#include <stddef.h>
#include <weivel/core.h>

/** One event the core queued, kept after the event itself is released. */
typedef struct {
    uint32_t kind;
    /** The want or watch handle the event names; 0 for a kind that names neither. */
    weivel_handle subject;
    weivel_handle plugin;
    weivel_handle implementation;
    weivel_handle provider;
    /** A weivel_change or a weivel_capability_change. */
    uint32_t change;
    /** A weivel_reach_kind for a reach refusal. */
    uint32_t reach_kind;
    /** The refusal code a refusal, a reach refusal or a want settlement carried. */
    uint32_t code;
    /** The service or capability id of a reach refusal, owned here. */
    char *id;
    /** The virtual time the adapter had handed the core when the event was drained. */
    int64_t at;
} heard_event;

/**
 * Everything the core told one run, in the order it was told: an adapter answers from this log and from the core's
 * returns, never from what it believes it asked for.
 * @implNote An activation check that refuses is quarantine, not a refusal of the run (RUNNER.md, `consume`), so
 * WEIVEL_REFUSAL_ACTIVATION_MISMATCH never becomes `first_refusal`.
 */
typedef struct {
    heard_event *events;
    size_t count;
    size_t capacity;
    int64_t now;
    uint32_t first_refusal;
    char *first_detail;
    char *first_fix;
    char *first_subject;
    uint32_t first_has_fix;
    int failed;
} hearing;

void hearing_init(hearing *log);
void hearing_release(hearing *log);

/** An event_reader: records what `event` says into the hearing passed as `context`. */
void hearing_read(const weivel_event *event, void *context);

/** The settlement of `want`, or NULL while the core has not settled it. */
const heard_event *hearing_settlement(const hearing *log, weivel_handle want);

/** 1 when the core reported a refusal of reach for `plugin` and `id`. */
int hearing_reported(const hearing *log, weivel_handle plugin, const char *id);

/** 1 when the core revoked `implementation` from `plugin`. */
int hearing_revoked(const hearing *log, weivel_handle plugin, weivel_handle implementation);

#endif
