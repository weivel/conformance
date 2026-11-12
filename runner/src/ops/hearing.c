#include "hearing.h"

#include "text.h"

#include <stdlib.h>
#include <string.h>

void hearing_init(hearing *log) {
    memset(log, 0, sizeof *log);
}

void hearing_release(hearing *log) {
    size_t at;
    for (at = 0; at < log->count; at++) {
        free(log->events[at].id);
    }
    free(log->events);
    free(log->first_detail);
    free(log->first_fix);
    hearing_init(log);
}

static heard_event *append(hearing *log) {
    heard_event *grown;
    size_t capacity;
    if (log->count == log->capacity) {
        capacity = log->capacity == 0 ? 16 : log->capacity * 2;
        grown = realloc(log->events, capacity * sizeof *grown);
        if (grown == NULL) {
            log->failed = 1;
            return NULL;
        }
        log->events = grown;
        log->capacity = capacity;
    }
    memset(&log->events[log->count], 0, sizeof log->events[log->count]);
    log->events[log->count].at = log->now;
    return &log->events[log->count++];
}

static char *copy_or_null(hearing *log, const char *text) {
    char *copy;
    if (text == NULL) {
        return NULL;
    }
    copy = text_duplicate(text);
    if (copy == NULL) {
        log->failed = 1;
    }
    return copy;
}

static void note_refusal(hearing *log, const weivel_refusal *refusal) {
    if (log->first_refusal != WEIVEL_REFUSAL_NONE || refusal->code == WEIVEL_REFUSAL_NONE ||
        refusal->code == WEIVEL_REFUSAL_ACTIVATION_MISMATCH) {
        return;
    }
    log->first_refusal = refusal->code;
    log->first_detail = copy_or_null(log, refusal->detail);
    log->first_fix = copy_or_null(log, refusal->fix);
}

static void record_want(heard_event *heard, const weivel_want_event *want) {
    heard->subject = want->want;
    heard->plugin = want->plugin;
    heard->implementation = want->implementation;
    heard->code = want->refusal.code;
}

static void record_watch(heard_event *heard, const weivel_watch_event *watch) {
    heard->subject = watch->watch;
    heard->plugin = watch->plugin;
    heard->implementation = watch->implementation;
    heard->change = watch->change;
}

static void record_capability(heard_event *heard, const weivel_capability_event *capability) {
    heard->subject = capability->watch;
    heard->plugin = capability->plugin;
    heard->implementation = capability->implementation;
    heard->provider = capability->provider;
    heard->change = capability->change;
}

static void record_reach(hearing *log, heard_event *heard, const weivel_reach_refusal *reach) {
    heard->plugin = reach->plugin;
    heard->reach_kind = reach->kind;
    heard->code = reach->refusal.code;
    heard->id = copy_or_null(log, reach->id);
}

void hearing_read(const weivel_event *event, void *context) {
    hearing *log = context;
    heard_event *heard = append(log);
    if (heard == NULL) {
        return;
    }
    heard->kind = event->kind;
    switch (event->kind) {
    case WEIVEL_EVENT_REFUSAL:
        heard->code = event->data.refusal.code;
        note_refusal(log, &event->data.refusal);
        break;
    case WEIVEL_EVENT_WANT_SETTLED:
        record_want(heard, &event->data.want);
        note_refusal(log, &event->data.want.refusal);
        break;
    case WEIVEL_EVENT_WATCH:
        record_watch(heard, &event->data.watch);
        break;
    case WEIVEL_EVENT_REVOKED:
        heard->plugin = event->data.revoked.plugin;
        heard->implementation = event->data.revoked.implementation;
        break;
    case WEIVEL_EVENT_REACH_REFUSED:
        record_reach(log, heard, &event->data.reach);
        note_refusal(log, &event->data.reach.refusal);
        break;
    case WEIVEL_EVENT_CAPABILITY:
        record_capability(heard, &event->data.capability);
        break;
    default:
        break;
    }
}

const heard_event *hearing_settlement(const hearing *log, weivel_handle want) {
    size_t at;
    for (at = 0; at < log->count; at++) {
        if (log->events[at].kind == WEIVEL_EVENT_WANT_SETTLED && log->events[at].subject == want) {
            return &log->events[at];
        }
    }
    return NULL;
}

int hearing_reported(const hearing *log, weivel_handle plugin, const char *id) {
    size_t at;
    for (at = 0; at < log->count; at++) {
        const heard_event *heard = &log->events[at];
        if (heard->kind == WEIVEL_EVENT_REACH_REFUSED && heard->plugin == plugin && heard->id != NULL &&
            strcmp(heard->id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

int hearing_revoked(const hearing *log, weivel_handle plugin, weivel_handle implementation) {
    size_t at;
    for (at = 0; at < log->count; at++) {
        const heard_event *heard = &log->events[at];
        if (heard->kind == WEIVEL_EVENT_REVOKED && heard->plugin == plugin &&
            heard->implementation == implementation) {
            return 1;
        }
    }
    return 0;
}
