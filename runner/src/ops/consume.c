#include "adapters.h"
#include "cast.h"
#include "facts.h"

#include <string.h>

/**
 * An implementation the core answered a `provide` with, and the plugin whose object it stands for.
 * @implNote The core holds no object, so the implementation handle is what names its provider: a consumer's answer
 * is the provider of each handle the core hands back, never inferred from the steps the adapter ran.
 */
typedef struct {
    weivel_handle implementation;
    weivel_handle provider;
    const char *provider_name;
} provided_object;

/** An `all` step's answer, or the watch a `watch` step installed (0 when the core left it inert). */
typedef struct {
    int is_watch;
    weivel_handle watch;
    const char **providers;
    uint32_t provider_count;
} consume_outcome;

typedef struct {
    core_session session;
    hearing log;
    arena owner;
    cast plugins;
    provided_object *provided;
    size_t provided_count;
    consume_outcome *outcomes;
    size_t outcome_count;
} consume_run;

static int open_plugins(consume_run *run, const json_value *plugins) {
    size_t count = json_array_length(plugins);
    const char **names = arena_take(&run->owner, count, sizeof *names);
    weivel_manifest *manifests = arena_take(&run->owner, count, sizeof *manifests);
    size_t at;
    if (plugins == NULL || json_kind_of(plugins) != JSON_ARRAY || count == 0 || names == NULL || manifests == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        const json_value *plugin = json_array_at(plugins, at);
        names[at] = json_as_string(json_object_get(plugin, "id"));
        if (names[at] == NULL || !facts_manifest(&run->owner, plugin, &manifests[at])) {
            return 0;
        }
    }
    return cast_open(&run->owner, &run->session, (const char *const *)names, manifests, (uint32_t)count,
                     &run->plugins);
}

static const provided_object *object_of(const consume_run *run, weivel_handle implementation) {
    size_t at;
    for (at = 0; at < run->provided_count; at++) {
        if (run->provided[at].implementation == implementation) {
            return &run->provided[at];
        }
    }
    return NULL;
}

/** The provider an implementation stands for, NULL when the core names a provider other than the one it was given. */
static const char *provider_of(const consume_run *run, weivel_handle implementation, weivel_handle provider) {
    const provided_object *object = object_of(run, implementation);
    return object != NULL && object->provider == provider ? object->provider_name : NULL;
}

static int provide(consume_run *run, weivel_handle plugin, const char *id) {
    provided_object *object = &run->provided[run->provided_count];
    weivel_status status = weivel_provide_capability(run->session.core, plugin, id, &object->implementation, NULL);
    session_drain(&run->session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    object->provider = plugin;
    object->provider_name = cast_name(&run->plugins, plugin);
    run->provided_count++;
    return 1;
}

/** A plugin whose activation check refuses is quarantined, as a host does with an activation it will not run. */
static int verify(consume_run *run, weivel_handle plugin) {
    weivel_status status = weivel_verify_activation(run->session.core, plugin, NULL);
    session_drain(&run->session);
    if (status == WEIVEL_STATUS_OK) {
        return 1;
    }
    if (status != WEIVEL_STATUS_REFUSED) {
        return 0;
    }
    status = weivel_quarantine_plugin(run->session.core, plugin, NULL, NULL);
    session_drain(&run->session);
    return status == WEIVEL_STATUS_OK;
}

static int keep_providers(consume_run *run, const weivel_capability_list *list, consume_outcome *outcome) {
    const char *item = (const char *)list->items;
    uint32_t stride = list->count > 0 ? list->items[0].size : 0;
    uint32_t at;
    outcome->providers = arena_take(&run->owner, list->count, sizeof *outcome->providers);
    outcome->provider_count = list->count;
    if (list->count > 0 && (outcome->providers == NULL || stride < sizeof(weivel_capability_implementation))) {
        return 0;
    }
    for (at = 0; at < list->count; at++) {
        const weivel_capability_implementation *found =
            (const weivel_capability_implementation *)(const void *)(item + (size_t)at * stride);
        outcome->providers[at] = provider_of(run, found->implementation, found->provider);
        if (outcome->providers[at] == NULL) {
            return 0;
        }
    }
    return 1;
}

/** An undeclared `all` is answered an empty list and refused, which RUNNER.md answers as `[]`. */
static int read_all(consume_run *run, weivel_handle plugin, const char *id) {
    consume_outcome *outcome = &run->outcomes[run->outcome_count++];
    weivel_capability_list list = WEIVEL_INIT(weivel_capability_list);
    weivel_status status = weivel_all_capabilities(run->session.core, plugin, id, &list, NULL);
    int kept;
    session_drain(&run->session);
    if (status == WEIVEL_STATUS_REFUSED && list.count == 0) {
        return 1;
    }
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    kept = keep_providers(run, &list, outcome);
    weivel_capability_list_release(&list);
    return kept;
}

/** An undeclared or late watch is refused and inert; it keeps watch 0 and hears nothing. */
static int watch(consume_run *run, weivel_handle plugin, const char *id) {
    consume_outcome *outcome = &run->outcomes[run->outcome_count++];
    weivel_status status = weivel_watch_capability(run->session.core, plugin, id, &outcome->watch, NULL);
    session_drain(&run->session);
    outcome->is_watch = 1;
    return status == WEIVEL_STATUS_OK || status == WEIVEL_STATUS_REFUSED;
}

static int stop(consume_run *run, weivel_handle plugin, int quarantine) {
    weivel_status status = quarantine ? weivel_quarantine_plugin(run->session.core, plugin, NULL, NULL)
                                      : weivel_release_plugin(run->session.core, plugin, NULL);
    session_drain(&run->session);
    return status == WEIVEL_STATUS_OK;
}

static int run_step(consume_run *run, const json_value *step) {
    weivel_handle plugin = cast_handle(&run->plugins, step_text(step, "plugin"));
    if (plugin == 0) {
        return 0;
    }
    if (step_text(step, "provide") != NULL) {
        return provide(run, plugin, step_text(step, "provide"));
    }
    if (step_text(step, "all") != NULL) {
        return read_all(run, plugin, step_text(step, "all"));
    }
    if (step_text(step, "watch") != NULL) {
        return watch(run, plugin, step_text(step, "watch"));
    }
    if (json_object_get(step, "verify") != NULL) {
        return verify(run, plugin);
    }
    if (json_object_get(step, "quarantine") != NULL || json_object_get(step, "release") != NULL) {
        return stop(run, plugin, json_object_get(step, "quarantine") != NULL);
    }
    return 0;
}

static int run_steps(consume_run *run, const json_value *given) {
    const json_value *steps = json_object_get(given, "steps");
    size_t count = json_array_length(steps);
    size_t at;
    if (steps == NULL || json_kind_of(steps) != JSON_ARRAY || count == 0) {
        return 0;
    }
    run->provided = arena_take(&run->owner, count, sizeof *run->provided);
    run->outcomes = arena_take(&run->owner, count, sizeof *run->outcomes);
    if (run->provided == NULL || run->outcomes == NULL || !open_plugins(run, json_object_get(given, "plugins"))) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (!run_step(run, json_array_at(steps, at))) {
            return 0;
        }
    }
    return 1;
}

static const char *change_name(uint32_t change) {
    if (change == WEIVEL_CAPABILITY_PROVIDED) {
        return "provide";
    }
    return change == WEIVEL_CAPABILITY_WITHDRAWN ? "withdraw" : NULL;
}

static int write_heard_by(answer_writer *answer, const consume_run *run, weivel_handle watch) {
    size_t at;
    answer_begin_array(answer);
    for (at = 0; watch != 0 && at < run->log.count; at++) {
        const heard_event *heard = &run->log.events[at];
        const char *provider;
        if (heard->kind != WEIVEL_EVENT_CAPABILITY || heard->subject != watch) {
            continue;
        }
        provider = provider_of(run, heard->implementation, heard->provider);
        if (provider == NULL || change_name(heard->change) == NULL) {
            return 0;
        }
        answer_begin_array(answer);
        answer_string(answer, provider);
        answer_string(answer, change_name(heard->change));
        answer_end_array(answer);
    }
    answer_end_array(answer);
    return 1;
}

static void write_providers(answer_writer *answer, const consume_outcome *outcome) {
    uint32_t at;
    answer_begin_array(answer);
    for (at = 0; at < outcome->provider_count; at++) {
        answer_string(answer, outcome->providers[at]);
    }
    answer_end_array(answer);
}

static int write_consumption(answer_writer *answer, const consume_run *run) {
    size_t at;
    int written = 1;
    answer_begin_object(answer);
    answer_key(answer, "outcomes");
    answer_begin_array(answer);
    for (at = 0; at < run->outcome_count; at++) {
        if (!run->outcomes[at].is_watch) {
            write_providers(answer, &run->outcomes[at]);
        }
    }
    answer_end_array(answer);
    answer_key(answer, "heard");
    answer_begin_array(answer);
    for (at = 0; at < run->outcome_count && written; at++) {
        if (run->outcomes[at].is_watch) {
            written = write_heard_by(answer, run, run->outcomes[at].watch);
        }
    }
    answer_end_array(answer);
    answer_key(answer, "reported");
    written = written && write_reported(answer, &run->log, &run->plugins);
    answer_key(answer, "refusal");
    write_first_refusal(answer, &run->log);
    answer_end_object(answer);
    return written;
}

int adapter_consume(const json_value *given, answer_writer *answer) {
    consume_run run;
    int answered = 0;
    memset(&run, 0, sizeof run);
    hearing_init(&run.log);
    arena_init(&run.owner);
    if (session_open(&run.session, NULL, hearing_read, &run.log)) {
        answered = run_steps(&run, given) && !run.log.failed && !run.session.failed && write_consumption(answer, &run);
        session_close(&run.session);
    }
    hearing_release(&run.log);
    arena_release(&run.owner);
    return answered;
}
