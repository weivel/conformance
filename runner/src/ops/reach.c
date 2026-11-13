#include "adapters.h"
#include "cast.h"
#include "facts.h"
#include "want_states.h"

#include <stdlib.h>
#include <string.h>

typedef enum { REACH_STEP_REGISTER, REACH_STEP_UNREGISTER, REACH_STEP_GET, REACH_STEP_WANT, REACH_STEP_WATCH,
               REACH_STEP_IDS } reach_step_kind;

/** What one step asked and what the core answered it, kept until the answer is written after the last step. */
typedef struct {
    reach_step_kind kind;
    weivel_handle plugin;
    const char *id;
    weivel_status status;
    /** A get's registration, or the want or watch the step started. */
    weivel_handle handle;
    const char **ids;
    uint32_t id_count;
    /** `served`, `absent` or `refused`, once the core has answered the step. */
    const char *settled;
    /** The registration that served the step, 0 when none did. */
    weivel_handle served;
} reach_outcome;

/**
 * A registration and the object it stands for, as a host keeps it.
 * @implNote The core holds no object: the registration handle it answers stands for one, so which object served a
 * step is read by looking up the handle the core handed back, never from which registration the script made last.
 */
typedef struct {
    weivel_handle registration;
    const char *id;
    const char *object;
    int by_host;
    int ended;
} reach_registration;

typedef struct {
    core_session session;
    hearing log;
    arena owner;
    cast plugins;
    reach_outcome *outcomes;
    size_t outcome_count;
    reach_registration *registrations;
    size_t registration_count;
} reach_run;

static int plugin_manifest(arena *owner, const weivel_json_value *plugin, weivel_manifest *manifest) {
    weivel_manifest built = WEIVEL_INIT(weivel_manifest);
    const weivel_json_value *import_name = weivel_json_object_get(plugin, "importName");
    *manifest = built;
    manifest->identity = weivel_json_as_string(weivel_json_object_get(plugin, "id"));
    manifest->import_name = import_name == NULL ? manifest->identity : weivel_json_as_string(import_name);
    return manifest->identity != NULL && manifest->import_name != NULL &&
           facts_service_wants(owner, weivel_json_object_get(plugin, "consumes"), &manifest->services.consumes) &&
           facts_service_offers(owner, weivel_json_object_get(plugin, "provides"), &manifest->services.provides);
}

static int open_plugins(reach_run *run, const weivel_json_value *plugins) {
    size_t count = weivel_json_array_length(plugins);
    const char **names = arena_take(&run->owner, count, sizeof *names);
    weivel_manifest *manifests = arena_take(&run->owner, count, sizeof *manifests);
    size_t at;
    if (plugins == NULL || weivel_json_kind_of(plugins) != WEIVEL_JSON_ARRAY || count == 0 || names == NULL || manifests == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (!plugin_manifest(&run->owner, weivel_json_array_at(plugins, at), &manifests[at])) {
            return 0;
        }
        names[at] = manifests[at].identity;
    }
    return cast_open(&run->owner, &run->session, (const char *const *)names, manifests, (uint32_t)count,
                     &run->plugins);
}

static void note_registration(reach_run *run, weivel_handle registration, const char *id, const char *object,
                              int by_host) {
    reach_registration *noted = &run->registrations[run->registration_count++];
    noted->registration = registration;
    noted->id = id;
    noted->object = object;
    noted->by_host = by_host;
    noted->ended = 0;
}

static int register_service(reach_run *run, weivel_handle owner, const char *id, const char *object) {
    weivel_handle registration = 0;
    weivel_status status = weivel_register_service(run->session.core, owner, id, &registration, NULL);
    session_drain(&run->session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    note_registration(run, registration, id, object, owner == WEIVEL_HOST);
    return 1;
}

/** The host's live registration of `id`, the one its own `unregister` ends. */
static reach_registration *host_registration(reach_run *run, const char *id) {
    size_t at = run->registration_count;
    while (at-- > 0) {
        reach_registration *noted = &run->registrations[at];
        if (noted->by_host && !noted->ended && strcmp(noted->id, id) == 0) {
            return noted;
        }
    }
    return NULL;
}

static int unregister_host_service(reach_run *run, const char *id) {
    reach_registration *noted = host_registration(run, id);
    weivel_status status;
    if (noted == NULL) {
        return 0;
    }
    status = weivel_unregister(run->session.core, noted->registration, NULL);
    session_drain(&run->session);
    noted->ended = 1;
    return status == WEIVEL_STATUS_OK;
}

static const char *object_of(const reach_run *run, weivel_handle registration) {
    size_t at;
    for (at = 0; registration != 0 && at < run->registration_count; at++) {
        if (run->registrations[at].registration == registration) {
            return run->registrations[at].object;
        }
    }
    return NULL;
}

static int compare_texts(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

static int keep_ids(reach_run *run, const weivel_service_list *list, reach_outcome *outcome) {
    uint32_t at;
    outcome->ids = arena_take(&run->owner, list->ids.count, sizeof *outcome->ids);
    outcome->id_count = list->ids.count;
    if (list->ids.count > 0 && outcome->ids == NULL) {
        return 0;
    }
    for (at = 0; at < list->ids.count; at++) {
        size_t length = strlen(list->ids.items[at]);
        char *copy = arena_take(&run->owner, length + 1, 1);
        if (copy == NULL) {
            return 0;
        }
        memcpy(copy, list->ids.items[at], length);
        outcome->ids[at] = copy;
    }
    qsort((void *)outcome->ids, outcome->id_count, sizeof *outcome->ids, compare_texts);
    return 1;
}

static int list_ids(reach_run *run, reach_outcome *outcome) {
    weivel_service_list list = WEIVEL_INIT(weivel_service_list);
    int kept;
    outcome->status = weivel_service_ids(run->session.core, outcome->plugin, &list, NULL);
    session_drain(&run->session);
    if (outcome->status != WEIVEL_STATUS_OK) {
        return 0;
    }
    kept = keep_ids(run, &list, outcome);
    weivel_service_list_release(&list);
    return kept;
}

static int refused_and_reported(const reach_run *run, const reach_outcome *outcome) {
    return outcome->status == WEIVEL_STATUS_REFUSED && hearing_reported(&run->log, outcome->plugin, outcome->id);
}

static const char *settle_get(reach_run *run, reach_outcome *outcome) {
    if (outcome->status == WEIVEL_STATUS_OK) {
        outcome->served = outcome->handle;
        return "served";
    }
    if (outcome->status == WEIVEL_STATUS_NONE) {
        return "absent";
    }
    return refused_and_reported(run, outcome) ? "refused" : NULL;
}

static int ask(reach_run *run, reach_outcome *outcome) {
    weivel_core *core = run->session.core;
    switch (outcome->kind) {
    case REACH_STEP_GET:
        outcome->status = weivel_get_service(core, outcome->plugin, outcome->id, &outcome->handle, NULL);
        break;
    case REACH_STEP_WANT:
        outcome->status = weivel_want_service(core, outcome->plugin, outcome->id, NULL, &outcome->handle, NULL);
        break;
    case REACH_STEP_WATCH:
        outcome->status = weivel_watch_service(core, outcome->plugin, outcome->id, &outcome->handle, NULL);
        break;
    default:
        return list_ids(run, outcome);
    }
    session_drain(&run->session);
    if (outcome->kind == REACH_STEP_GET) {
        outcome->settled = settle_get(run, outcome);
        return outcome->settled != NULL;
    }
    return outcome->kind != REACH_STEP_WANT || outcome->status == WEIVEL_STATUS_OK;
}

static int read_step_kind(const weivel_json_value *step, reach_step_kind *kind, const char **id) {
    static const char *const KEYS[] = {"register", "unregister", "get", "want", "watch"};
    size_t at;
    for (at = 0; at < sizeof KEYS / sizeof KEYS[0]; at++) {
        *id = step_text(step, KEYS[at]);
        if (*id != NULL) {
            *kind = (reach_step_kind)at;
            return 1;
        }
    }
    *kind = REACH_STEP_IDS;
    return weivel_json_object_get(step, "ids") != NULL;
}

static int is_host_step(const weivel_json_value *step) {
    int host = 0;
    return weivel_json_as_bool(weivel_json_object_get(step, "host"), &host) && host;
}

static int host_step(reach_run *run, const weivel_json_value *step, reach_step_kind kind, const char *id) {
    if (kind == REACH_STEP_REGISTER) {
        return register_service(run, WEIVEL_HOST, id, step_text(step, "object"));
    }
    return kind == REACH_STEP_UNREGISTER && unregister_host_service(run, id);
}

static int plugin_step(reach_run *run, const weivel_json_value *step, reach_step_kind kind, const char *id) {
    weivel_handle plugin = cast_handle(&run->plugins, step_text(step, "plugin"));
    reach_outcome *outcome = &run->outcomes[run->outcome_count];
    if (plugin == 0 || kind == REACH_STEP_UNREGISTER) {
        return 0;
    }
    if (kind == REACH_STEP_REGISTER) {
        return register_service(run, plugin, id, step_text(step, "object"));
    }
    outcome->kind = kind;
    outcome->plugin = plugin;
    outcome->id = id;
    run->outcome_count++;
    return ask(run, outcome);
}

static int run_step(reach_run *run, const weivel_json_value *step) {
    reach_step_kind kind;
    const char *id;
    if (!read_step_kind(step, &kind, &id)) {
        return 0;
    }
    return is_host_step(step) ? host_step(run, step, kind, id) : plugin_step(run, step, kind, id);
}

static int register_host_provided(reach_run *run, const weivel_json_value *ids) {
    size_t at;
    if (ids != NULL && weivel_json_kind_of(ids) != WEIVEL_JSON_ARRAY) {
        return 0;
    }
    for (at = 0; at < weivel_json_array_length(ids); at++) {
        const char *id = weivel_json_as_string(weivel_json_array_at(ids, at));
        if (id == NULL || !register_service(run, WEIVEL_HOST, id, NULL)) {
            return 0;
        }
    }
    return 1;
}

static int prepare(reach_run *run, const weivel_json_value *given) {
    const weivel_json_value *steps = weivel_json_object_get(given, "steps");
    size_t step_count = weivel_json_array_length(steps);
    size_t registrations = step_count + weivel_json_array_length(weivel_json_object_get(given, "hostProvided"));
    if (steps == NULL || weivel_json_kind_of(steps) != WEIVEL_JSON_ARRAY || step_count == 0) {
        return 0;
    }
    run->outcomes = arena_take(&run->owner, step_count, sizeof *run->outcomes);
    run->registrations = arena_take(&run->owner, registrations, sizeof *run->registrations);
    return run->outcomes != NULL && run->registrations != NULL &&
           open_plugins(run, weivel_json_object_get(given, "plugins")) &&
           register_host_provided(run, weivel_json_object_get(given, "hostProvided"));
}

static int run_steps(reach_run *run, const weivel_json_value *steps) {
    size_t at;
    for (at = 0; at < weivel_json_array_length(steps); at++) {
        if (!run_step(run, weivel_json_array_at(steps, at))) {
            return 0;
        }
    }
    return 1;
}

/** The registration a watch heard registered first, or 0 when it heard none. */
static weivel_handle first_heard(const hearing *log, weivel_handle watch) {
    size_t at;
    for (at = 0; at < log->count; at++) {
        const heard_event *heard = &log->events[at];
        if (heard->kind == WEIVEL_EVENT_WATCH && heard->subject == watch && heard->change == WEIVEL_CHANGE_REGISTERED) {
            return heard->implementation;
        }
    }
    return 0;
}

static const char *settle_want(reach_run *run, reach_outcome *outcome) {
    const heard_event *settlement = hearing_settlement(&run->log, outcome->handle);
    if (settlement != NULL) {
        outcome->served = settlement->implementation;
        return want_state_in_reach(settlement);
    }
    return session_still_kept(&run->session, outcome->handle) ? "absent" : NULL;
}

static const char *settle_watch(reach_run *run, reach_outcome *outcome) {
    if (outcome->status != WEIVEL_STATUS_OK) {
        return refused_and_reported(run, outcome) ? "refused" : NULL;
    }
    outcome->served = first_heard(&run->log, outcome->handle);
    if (outcome->served != 0) {
        return "served";
    }
    return session_still_kept(&run->session, outcome->handle) ? "absent" : NULL;
}

/** Settles each want and watch after the last step, as RUNNER.md reads them; 0 when the core gave no answer. */
static int settle_waiting(reach_run *run) {
    size_t at;
    for (at = 0; at < run->outcome_count; at++) {
        reach_outcome *outcome = &run->outcomes[at];
        if (outcome->kind == REACH_STEP_WANT) {
            outcome->settled = settle_want(run, outcome);
        } else if (outcome->kind == REACH_STEP_WATCH) {
            outcome->settled = settle_watch(run, outcome);
        }
        if (outcome->kind != REACH_STEP_IDS && outcome->settled == NULL) {
            return 0;
        }
    }
    return 1;
}

static void write_ids(answer_writer *answer, const reach_outcome *outcome) {
    uint32_t at;
    answer_begin_array(answer);
    for (at = 0; at < outcome->id_count; at++) {
        answer_string(answer, outcome->ids[at]);
    }
    answer_end_array(answer);
}

static void write_outcomes(answer_writer *answer, const reach_run *run) {
    size_t at;
    answer_begin_array(answer);
    for (at = 0; at < run->outcome_count; at++) {
        if (run->outcomes[at].kind == REACH_STEP_IDS) {
            write_ids(answer, &run->outcomes[at]);
        } else {
            answer_string(answer, run->outcomes[at].settled);
        }
    }
    answer_end_array(answer);
}

static void write_objects(answer_writer *answer, const reach_run *run) {
    size_t at;
    answer_begin_array(answer);
    for (at = 0; at < run->outcome_count; at++) {
        if (run->outcomes[at].kind != REACH_STEP_IDS) {
            write_text_or_null(answer, object_of(run, run->outcomes[at].served));
        }
    }
    answer_end_array(answer);
}

static int write_reach(answer_writer *answer, const reach_run *run) {
    int written;
    answer_begin_object(answer);
    answer_key(answer, "outcomes");
    write_outcomes(answer, run);
    answer_key(answer, "objects");
    write_objects(answer, run);
    answer_key(answer, "reported");
    written = write_reported(answer, &run->log, &run->plugins);
    answer_key(answer, "refusal");
    write_first_refusal(answer, &run->log);
    answer_end_object(answer);
    return written;
}

int adapter_reach(const weivel_json_value *given, answer_writer *answer) {
    reach_run run;
    int answered = 0;
    memset(&run, 0, sizeof run);
    hearing_init(&run.log);
    arena_init(&run.owner);
    if (session_open(&run.session, NULL, hearing_read, &run.log)) {
        answered = prepare(&run, given) && run_steps(&run, weivel_json_object_get(given, "steps")) &&
                   settle_waiting(&run) && !run.log.failed && !run.session.failed && write_reach(answer, &run);
        session_close(&run.session);
    }
    hearing_release(&run.log);
    arena_release(&run.owner);
    return answered;
}
