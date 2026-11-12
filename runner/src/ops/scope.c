#include "adapters.h"
#include "cast.h"
#include "want_states.h"

#include <string.h>

/**
 * The consumers a scope script names, each declaring what the script has it register and want.
 * @implNote The script states no manifest, and the reach fence is not what scope asks about, so each consumer
 * declares exactly the ids its own steps use; an undeclared want would be rejected for a reason unrelated to
 * invariant 11.
 */
typedef struct {
    const char **names;
    weivel_manifest *manifests;
    uint32_t count;
} scope_consumers;

static const char *consumer_of(const json_value *step) {
    const char *consumer = step_text(step, "consumer");
    return consumer != NULL ? consumer : step_text(step, "release");
}

static int index_of(const scope_consumers *consumers, const char *name, uint32_t *found) {
    uint32_t at;
    for (at = 0; at < consumers->count; at++) {
        if (strcmp(consumers->names[at], name) == 0) {
            *found = at;
            return 1;
        }
    }
    return 0;
}

static uint32_t count_steps(const json_value *steps, const char *name, const char *key) {
    uint32_t count = 0;
    size_t at;
    for (at = 0; at < json_array_length(steps); at++) {
        const json_value *step = json_array_at(steps, at);
        const char *consumer = step_text(step, "consumer");
        count += (uint32_t)(consumer != NULL && strcmp(consumer, name) == 0 && step_text(step, key) != NULL);
    }
    return count;
}

static int declare(arena *owner, const json_value *steps, const char *name, weivel_manifest *manifest) {
    uint32_t offers = count_steps(steps, name, "register");
    uint32_t wants = count_steps(steps, name, "want");
    weivel_service_offer *offer = arena_take(owner, offers, sizeof *offer);
    weivel_service_want *want = arena_take(owner, wants, sizeof *want);
    weivel_manifest built = WEIVEL_INIT(weivel_manifest);
    uint32_t offered = 0;
    uint32_t wanted = 0;
    size_t at;
    *manifest = built;
    manifest->import_name = name;
    if ((offers > 0 && offer == NULL) || (wants > 0 && want == NULL)) {
        return 0;
    }
    for (at = 0; at < json_array_length(steps); at++) {
        const json_value *step = json_array_at(steps, at);
        const char *consumer = step_text(step, "consumer");
        if (consumer == NULL || strcmp(consumer, name) != 0) {
            continue;
        }
        if (step_text(step, "register") != NULL) {
            offer[offered].size = (uint32_t)sizeof offer[offered];
            offer[offered++].id = step_text(step, "register");
        } else if (step_text(step, "want") != NULL) {
            want[wanted].size = (uint32_t)sizeof want[wanted];
            want[wanted++].id = step_text(step, "want");
        }
    }
    manifest->services.provides.items = offer;
    manifest->services.provides.count = offers;
    manifest->services.consumes.items = want;
    manifest->services.consumes.count = wants;
    return 1;
}

static int gather_consumers(arena *owner, const json_value *steps, scope_consumers *consumers) {
    size_t length = json_array_length(steps);
    size_t at;
    uint32_t found;
    consumers->count = 0;
    consumers->names = arena_take(owner, length, sizeof *consumers->names);
    consumers->manifests = arena_take(owner, length, sizeof *consumers->manifests);
    if (steps == NULL || json_kind_of(steps) != JSON_ARRAY || length == 0 || consumers->names == NULL ||
        consumers->manifests == NULL) {
        return 0;
    }
    for (at = 0; at < length; at++) {
        const char *name = consumer_of(json_array_at(steps, at));
        if (name != NULL && !index_of(consumers, name, &found)) {
            consumers->names[consumers->count++] = name;
        }
    }
    for (at = 0; at < consumers->count; at++) {
        if (!declare(owner, steps, consumers->names[at], &consumers->manifests[at])) {
            return 0;
        }
    }
    return 1;
}

/** What one step of the script came to: a lookup's answer, or the want it started. */
typedef struct {
    int is_lookup;
    int found;
    weivel_handle want;
} scope_outcome;

static int lookup_step(core_session *session, const char *id, scope_outcome *outcome) {
    weivel_handle implementation = 0;
    weivel_status status = weivel_get_service(session->core, WEIVEL_HOST, id, &implementation, NULL);
    session_drain(session);
    outcome->is_lookup = 1;
    outcome->found = status == WEIVEL_STATUS_OK;
    return status == WEIVEL_STATUS_OK || status == WEIVEL_STATUS_NONE;
}

static int consumer_step(core_session *session, const cast *opened, const json_value *step, scope_outcome *outcome) {
    weivel_handle plugin = cast_handle(opened, step_text(step, "consumer"));
    weivel_handle registration = 0;
    weivel_status status;
    if (step_text(step, "register") != NULL) {
        status = weivel_register_service(session->core, plugin, step_text(step, "register"), &registration, NULL);
    } else if (step_text(step, "want") != NULL) {
        status = weivel_want_service(session->core, plugin, step_text(step, "want"), NULL, &outcome->want, NULL);
    } else {
        return 0;
    }
    session_drain(session);
    return plugin != 0 && status == WEIVEL_STATUS_OK;
}

static int run_step(core_session *session, const cast *opened, const json_value *step, scope_outcome *outcome) {
    weivel_status status;
    if (step_text(step, "lookup") != NULL) {
        return lookup_step(session, step_text(step, "lookup"), outcome);
    }
    if (step_text(step, "release") != NULL) {
        status = weivel_release_plugin(session->core, cast_handle(opened, step_text(step, "release")), NULL);
        session_drain(session);
        return status == WEIVEL_STATUS_OK;
    }
    return consumer_step(session, opened, step, outcome);
}

static const char *want_state(core_session *session, const hearing *log, weivel_handle want) {
    const heard_event *settlement = hearing_settlement(log, want);
    if (settlement != NULL) {
        return want_state_in_scope(settlement);
    }
    return session_still_kept(session, want) ? "pending" : NULL;
}

static int write_scope(answer_writer *answer, core_session *session, const hearing *log,
                       const scope_outcome *outcomes, size_t count) {
    size_t at;
    answer_begin_object(answer);
    answer_key(answer, "lookups");
    answer_begin_array(answer);
    for (at = 0; at < count; at++) {
        if (outcomes[at].is_lookup) {
            answer_bool(answer, outcomes[at].found);
        }
    }
    answer_end_array(answer);
    answer_key(answer, "wants");
    answer_begin_array(answer);
    for (at = 0; at < count; at++) {
        const char *state;
        if (outcomes[at].want == 0) {
            continue;
        }
        state = want_state(session, log, outcomes[at].want);
        if (state == NULL) {
            return 0;
        }
        answer_string(answer, state);
    }
    answer_end_array(answer);
    answer_end_object(answer);
    return 1;
}

static int run_script(core_session *session, hearing *log, arena *owner, const json_value *steps,
                      answer_writer *answer) {
    scope_consumers consumers;
    cast opened;
    scope_outcome *outcomes;
    size_t at;
    if (!gather_consumers(owner, steps, &consumers) ||
        !cast_open(owner, session, (const char *const *)consumers.names, consumers.manifests, consumers.count, &opened)) {
        return 0;
    }
    outcomes = arena_take(owner, json_array_length(steps), sizeof *outcomes);
    if (outcomes == NULL) {
        return 0;
    }
    for (at = 0; at < json_array_length(steps); at++) {
        if (!run_step(session, &opened, json_array_at(steps, at), &outcomes[at])) {
            return 0;
        }
    }
    return write_scope(answer, session, log, outcomes, json_array_length(steps)) && !log->failed &&
           !session->failed;
}

int adapter_scope(const json_value *given, answer_writer *answer) {
    core_session session;
    hearing log;
    arena owner;
    int answered = 0;
    hearing_init(&log);
    arena_init(&owner);
    if (session_open(&session, NULL, hearing_read, &log)) {
        answered = run_script(&session, &log, &owner, json_object_get(given, "steps"), answer);
        session_close(&session);
    }
    hearing_release(&log);
    arena_release(&owner);
    return answered;
}
