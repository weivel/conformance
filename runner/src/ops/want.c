#include "adapters.h"
#include "cast.h"

#include <string.h>

#define WANT_TICK_LIMIT 10000

/** One consumer waiting on a service a provider registers at a scripted moment, on a clock the adapter keeps. */
typedef struct {
    core_session session;
    hearing log;
    const char *service_id;
    weivel_handle provider;
    weivel_handle consumer;
    weivel_handle want;
    int registers;
    int64_t register_at;
    int timed;
    int64_t timeout_millis;
    int64_t horizon;
} want_run;

static int optional_millis(const weivel_json_value *value, int *present, int64_t *millis) {
    *present = 0;
    *millis = 0;
    if (value == NULL || weivel_json_kind_of(value) == WEIVEL_JSON_NULL) {
        return 1;
    }
    *present = 1;
    return weivel_json_as_int64(value, millis);
}

static int read_script(want_run *run, const weivel_json_value *given) {
    run->service_id = weivel_json_as_string(weivel_json_object_get(weivel_json_object_get(given, "service"), "id"));
    return run->service_id != NULL &&
           optional_millis(weivel_json_object_get(given, "registerAtMillis"), &run->registers, &run->register_at) &&
           optional_millis(weivel_json_object_get(given, "timeoutMillis"), &run->timed, &run->timeout_millis) &&
           weivel_json_as_int64(weivel_json_object_get(given, "horizonMillis"), &run->horizon);
}

static int advance_to(want_run *run, int64_t now) {
    weivel_status status;
    run->log.now = now;
    status = weivel_advance(run->session.core, now, NULL);
    session_drain(&run->session);
    return status == WEIVEL_STATUS_OK;
}

static int ask(want_run *run) {
    weivel_want_options options = WEIVEL_INIT(weivel_want_options);
    weivel_status status;
    options.now = 0;
    if (run->timed) {
        options.timed = WEIVEL_CHOICE_YES;
        options.timeout_millis = run->timeout_millis;
    }
    status = weivel_want_service(run->session.core, run->consumer, run->service_id, &options, &run->want, NULL);
    session_drain(&run->session);
    return status == WEIVEL_STATUS_OK;
}

static int register_now(want_run *run) {
    weivel_handle registration = 0;
    weivel_status status =
        weivel_register_service(run->session.core, run->provider, run->service_id, &registration, NULL);
    session_drain(&run->session);
    run->registers = 0;
    return status == WEIVEL_STATUS_OK;
}

static int64_t next_moment(want_run *run) {
    int64_t deadline = 0;
    int64_t next = run->horizon;
    if (weivel_next_deadline(run->session.core, &deadline, NULL) == WEIVEL_STATUS_OK && deadline < next) {
        next = deadline;
    }
    return next;
}

/** Hands the core every moment up to the horizon at which something is due: a deadline it set, or the registration. */
static int run_clock(want_run *run) {
    uint32_t tick;
    for (tick = 0; tick < WANT_TICK_LIMIT; tick++) {
        int64_t next = next_moment(run);
        if (run->registers && run->register_at <= next) {
            if (!advance_to(run, run->register_at) || !register_now(run)) {
                return 0;
            }
            continue;
        }
        if (!advance_to(run, next)) {
            return 0;
        }
        if (next >= run->horizon) {
            return 1;
        }
    }
    return 0;
}

static void write_outcome(answer_writer *answer, const want_run *run, int settled, int timed_out,
                          const heard_event *settlement) {
    answer_begin_object(answer);
    answer_key(answer, "settled");
    answer_bool(answer, settled);
    answer_key(answer, "timedOut");
    answer_bool(answer, timed_out);
    answer_key(answer, "atMillis");
    if (settlement == NULL) {
        answer_null(answer);
    } else {
        answer_int(answer, settlement->at);
    }
    answer_key(answer, "refusal");
    write_first_refusal(answer, &run->log);
    answer_end_object(answer);
}

/** A want the core neither settled nor still keeps, or one rejected for anything but time, has no answer here. */
static int write_settlement(answer_writer *answer, want_run *run) {
    const heard_event *settlement = hearing_settlement(&run->log, run->want);
    if (settlement == NULL) {
        if (!session_still_live(&run->session, run->want)) {
            return 0;
        }
        write_outcome(answer, run, 0, 0, NULL);
        return 1;
    }
    if (settlement->implementation != 0) {
        write_outcome(answer, run, 1, 0, settlement);
        return 1;
    }
    if (settlement->code == WEIVEL_REFUSAL_TIMED_OUT) {
        write_outcome(answer, run, 0, 1, settlement);
        return 1;
    }
    return 0;
}

int adapter_want(const weivel_json_value *given, answer_writer *answer) {
    want_run run;
    arena owner;
    int answered = 0;
    memset(&run, 0, sizeof run);
    hearing_init(&run.log);
    arena_init(&owner);
    if (read_script(&run, given) &&
        session_open_pair(&run.session, &run.log, &owner, run.service_id, &run.provider, &run.consumer) &&
        ask(&run) && run_clock(&run) && !run.log.failed) {
        answered = write_settlement(answer, &run) && !run.session.failed;
    }
    if (run.session.core != NULL) {
        session_close(&run.session);
    }
    hearing_release(&run.log);
    arena_release(&owner);
    return answered;
}
