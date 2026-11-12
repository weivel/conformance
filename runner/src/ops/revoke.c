#include "adapters.h"
#include "cast.h"

#include <string.h>

#define REVOKE_MAXIMUM_LOOKUPS 64

/** A provider and a consumer of one service, and what the consumer was handed and told. */
typedef struct {
    core_session session;
    hearing log;
    const char *service_id;
    weivel_handle provider;
    weivel_handle consumer;
    weivel_handle registration;
    weivel_handle held;
    int lookups[REVOKE_MAXIMUM_LOOKUPS];
    uint32_t lookup_count;
    int used_at_all;
    int used;
} revoke_run;

static int register_step(revoke_run *run) {
    weivel_status status;
    if (run->registration != 0) {
        return 0;
    }
    status = weivel_register_service(run->session.core, run->provider, run->service_id, &run->registration, NULL);
    session_drain(&run->session);
    return status == WEIVEL_STATUS_OK;
}

static int lookup_step(revoke_run *run) {
    weivel_handle implementation = 0;
    weivel_status status =
        weivel_get_service(run->session.core, run->consumer, run->service_id, &implementation, NULL);
    session_drain(&run->session);
    if (run->lookup_count == REVOKE_MAXIMUM_LOOKUPS || (status != WEIVEL_STATUS_OK && status != WEIVEL_STATUS_NONE)) {
        return 0;
    }
    run->lookups[run->lookup_count++] = status == WEIVEL_STATUS_OK;
    if (status == WEIVEL_STATUS_OK) {
        run->held = implementation;
    }
    return 1;
}

static int revoke_step(revoke_run *run) {
    weivel_status status = weivel_unregister(run->session.core, run->registration, NULL);
    session_drain(&run->session);
    return run->registration != 0 && status == WEIVEL_STATUS_OK;
}

/**
 * @implNote A consumer learns its handle is gone only from the core's revocation, so a handle never revoked counts
 * as usable only once the core hands the very same registration back.
 */
static int use_step(revoke_run *run) {
    weivel_handle current = 0;
    weivel_status status;
    if (run->held == 0) {
        return 0;
    }
    run->used_at_all = 1;
    if (hearing_revoked(&run->log, run->consumer, run->held)) {
        run->used = 0;
        return 1;
    }
    status = weivel_get_service(run->session.core, run->consumer, run->service_id, &current, NULL);
    session_drain(&run->session);
    run->used = 1;
    return status == WEIVEL_STATUS_OK && current == run->held;
}

static int run_step(revoke_run *run, const char *step) {
    if (step == NULL) {
        return 0;
    }
    if (strcmp(step, "register") == 0) {
        return register_step(run);
    }
    if (strcmp(step, "lookup") == 0) {
        return lookup_step(run);
    }
    if (strcmp(step, "revoke") == 0) {
        return revoke_step(run);
    }
    if (strcmp(step, "use") == 0) {
        return use_step(run);
    }
    return 0;
}

static void write_revocation(answer_writer *answer, const revoke_run *run) {
    uint32_t at;
    answer_begin_object(answer);
    answer_key(answer, "lookups");
    answer_begin_array(answer);
    for (at = 0; at < run->lookup_count; at++) {
        answer_bool(answer, run->lookups[at]);
    }
    answer_end_array(answer);
    if (run->used_at_all) {
        answer_key(answer, "used");
        answer_bool(answer, run->used);
        answer_key(answer, "reason");
        write_text_or_null(answer, run->used ? NULL : "revoked");
    }
    answer_end_object(answer);
}

static int run_steps(revoke_run *run, const json_value *steps) {
    size_t at;
    if (steps == NULL || json_kind_of(steps) != JSON_ARRAY) {
        return 0;
    }
    for (at = 0; at < json_array_length(steps); at++) {
        if (!run_step(run, json_as_string(json_array_at(steps, at)))) {
            return 0;
        }
    }
    return 1;
}

int adapter_revoke(const json_value *given, answer_writer *answer) {
    revoke_run run;
    arena owner;
    int answered = 0;
    memset(&run, 0, sizeof run);
    hearing_init(&run.log);
    arena_init(&owner);
    run.service_id = json_as_string(json_object_get(json_object_get(given, "service"), "id"));
    if (run.service_id != NULL &&
        session_open_pair(&run.session, &run.log, &owner, run.service_id, &run.provider, &run.consumer)) {
        answered = run_steps(&run, json_object_get(given, "steps")) && !run.log.failed && !run.session.failed;
    }
    if (run.session.core != NULL) {
        session_close(&run.session);
    }
    if (answered) {
        write_revocation(answer, &run);
    }
    hearing_release(&run.log);
    arena_release(&owner);
    return answered;
}
