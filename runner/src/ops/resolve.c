#include "adapters.h"
#include "facts.h"
#include "session.h"
#include "text.h"

#include <stdlib.h>
#include <string.h>

#define RESOLVE_MAXIMUM_ASKS 64
#define RESOLVE_TICK_LIMIT 10000

/** What the scripted provider was asked and what the consumer's want came to, read from the core's events. */
typedef struct {
    core_session session;
    weivel_handle want;
    int64_t now;
    const weivel_json_value *attributes_by_poll;
    int serves_only_unconstrained;
    uint32_t reads;
    uint32_t reads_at_service;
    int asks[RESOLVE_MAXIMUM_ASKS];
    uint32_t ask_count;
    int settled;
    int degraded;
    weivel_handle implementation;
    uint32_t code;
    char *detail;
    char *fix;
    char *log_just_before;
    char *report;
    int failed;
} resolve_run;

static int is_array(const weivel_json_value *value) {
    return value != NULL && weivel_json_kind_of(value) == WEIVEL_JSON_ARRAY;
}

static int bound_of(const weivel_json_value *named, uint32_t *bound) {
    const char *text = weivel_json_as_string(named);
    if (text != NULL && strcmp(text, "lower") == 0) {
        *bound = WEIVEL_BOUND_LOWER;
        return 1;
    }
    if (text != NULL && strcmp(text, "upper") == 0) {
        *bound = WEIVEL_BOUND_UPPER;
        return 1;
    }
    return 0;
}

static int vocabulary_entry(const weivel_json_value *object, weivel_constraint_entry *entry) {
    entry->size = (uint32_t)sizeof *entry;
    entry->constraint_name = weivel_json_as_string(weivel_json_object_get(object, "constraint"));
    entry->attribute_name = weivel_json_as_string(weivel_json_object_get(object, "attribute"));
    return entry->constraint_name != NULL && entry->attribute_name != NULL &&
           bound_of(weivel_json_object_get(object, "bound"), &entry->bound);
}

/** The host's vocabulary, which the vector states: the core mints no constraint of its own. */
static int host_vocabulary(arena *owner, const weivel_json_value *array, weivel_constraint_vocabulary *vocabulary) {
    size_t count = weivel_json_array_length(array);
    weivel_constraint_entry *entries = arena_take(owner, count, sizeof *entries);
    size_t at;
    vocabulary->items = entries;
    vocabulary->count = (uint32_t)count;
    if (!is_array(array) || count > UINT32_MAX || (count > 0 && entries == NULL)) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (!vocabulary_entry(weivel_json_array_at(array, at), &entries[at])) {
            return 0;
        }
    }
    return 1;
}

/**
 * @implNote Entry i answers poll i, and a budget that outlasts the script keeps the last entry, so the provider
 * goes on reporting its latest state rather than reverting to its advertised claim, which another vector covers.
 */
static const weivel_json_value *scripted_attributes(const resolve_run *run) {
    size_t polls = weivel_json_array_length(run->attributes_by_poll);
    size_t read = run->reads > polls ? polls : run->reads;
    return read == 0 ? NULL : weivel_json_array_at(run->attributes_by_poll, read - 1);
}

static void report_capacity(resolve_run *run, const weivel_capacity_event *asked, weivel_capacity *capacity) {
    weivel_status status;
    capacity->request = asked->request;
    status = weivel_report_capacity(run->session.core, asked->want, asked->registration, capacity, run->now, NULL);
    if (status != WEIVEL_STATUS_OK) {
        run->failed = 1;
    }
}

static void answer_attributes(resolve_run *run, const weivel_capacity_event *asked) {
    arena owner;
    weivel_capacity capacity = WEIVEL_INIT(weivel_capacity);
    arena_init(&owner);
    run->reads++;
    if (facts_numbers(&owner, scripted_attributes(run), &capacity.attributes)) {
        report_capacity(run, asked, &capacity);
    } else {
        run->failed = 1;
    }
    arena_release(&owner);
}

static void answer_serve(resolve_run *run, const weivel_capacity_event *asked) {
    weivel_capacity capacity = WEIVEL_INIT(weivel_capacity);
    int constrained = asked->constraints.count > 0;
    int serves = !(run->serves_only_unconstrained && constrained);
    if (run->ask_count == RESOLVE_MAXIMUM_ASKS) {
        run->failed = 1;
        return;
    }
    run->asks[run->ask_count++] = constrained;
    if (serves) {
        run->reads_at_service = run->reads;
    }
    capacity.serves = serves ? WEIVEL_CHOICE_YES : WEIVEL_CHOICE_NO;
    report_capacity(run, asked, &capacity);
}

static void read_capacity_question(resolve_run *run, const weivel_capacity_event *asked) {
    if (asked->want != run->want) {
        return;
    }
    if (asked->question == WEIVEL_CAPACITY_ATTRIBUTES) {
        answer_attributes(run, asked);
    } else if (asked->question == WEIVEL_CAPACITY_SERVE) {
        answer_serve(run, asked);
    } else {
        run->failed = 1;
    }
}

static void read_settlement(resolve_run *run, const weivel_want_event *settled) {
    if (settled->want != run->want) {
        return;
    }
    run->settled = 1;
    run->degraded = settled->degraded != 0;
    run->implementation = settled->implementation;
    run->code = settled->refusal.code;
    run->detail = settled->refusal.detail == NULL ? NULL : text_duplicate(settled->refusal.detail);
    run->fix = settled->refusal.fix == NULL ? NULL : text_duplicate(settled->refusal.fix);
    if (run->degraded) {
        run->report = run->log_just_before;
        run->log_just_before = NULL;
    }
}

static void forget_log(resolve_run *run) {
    free(run->log_just_before);
    run->log_just_before = NULL;
}

/** @implNote The core tells a degraded want's consumer why in the warning that comes just before its settlement. */
static void read_event(const weivel_event *event, void *context) {
    resolve_run *run = context;
    if (event->kind == WEIVEL_EVENT_LOG) {
        forget_log(run);
        if (event->data.log.level == WEIVEL_LOG_WARNING && event->data.log.text != NULL) {
            run->log_just_before = text_duplicate(event->data.log.text);
        }
        return;
    }
    if (event->kind == WEIVEL_EVENT_WANT_SETTLED) {
        read_settlement(run, &event->data.want);
    }
    forget_log(run);
    if (event->kind == WEIVEL_EVENT_CAPACITY_WANTED) {
        read_capacity_question(run, &event->data.capacity);
    }
}

/** The vector's requirement and provider, as the manifests a host would load for the two plugins. */
typedef struct {
    const char *service_id;
    weivel_service_offer offer;
    weivel_service_want need;
    weivel_manifest provider;
    weivel_manifest consumer;
    weivel_strings well_known_services;
    weivel_constraint_vocabulary vocabulary;
    int64_t budget_millis;
    int64_t poll_millis;
} resolve_setup;

static int read_setup(arena *owner, const weivel_json_value *given, resolve_setup *setup) {
    const weivel_json_value *requirement = weivel_json_object_get(given, "requirement");
    const weivel_json_value *provider = weivel_json_object_get(given, "provider");
    weivel_service_offer offer = WEIVEL_INIT(weivel_service_offer);
    weivel_service_want need = WEIVEL_INIT(weivel_service_want);
    weivel_manifest manifest = WEIVEL_INIT(weivel_manifest);
    setup->service_id = weivel_json_as_string(weivel_json_object_get(requirement, "id"));
    setup->offer = offer;
    setup->need = need;
    setup->provider = manifest;
    setup->consumer = manifest;
    setup->offer.id = setup->service_id;
    setup->need.id = setup->service_id;
    setup->need.optional = WEIVEL_CHOICE_NO;
    setup->provider.import_name = "provider";
    setup->provider.services.provides.items = &setup->offer;
    setup->provider.services.provides.count = 1;
    setup->consumer.import_name = "consumer";
    setup->consumer.services.consumes.items = &setup->need;
    setup->consumer.services.consumes.count = 1;
    return setup->service_id != NULL && weivel_json_as_int64(weivel_json_object_get(given, "budgetMillis"), &setup->budget_millis) &&
           weivel_json_as_int64(weivel_json_object_get(given, "pollMillis"), &setup->poll_millis) && setup->poll_millis > 0 &&
           facts_numbers(owner, weivel_json_object_get(provider, "advertised"), &setup->offer.attributes) &&
           facts_numbers(owner, weivel_json_object_get(requirement, "constraints"), &setup->need.constraints) &&
           facts_choice(weivel_json_object_get(requirement, "degradable"), &setup->need.degradable) &&
           is_array(weivel_json_object_get(given, "wellKnownServices")) &&
           facts_strings(owner, weivel_json_object_get(given, "wellKnownServices"), &setup->well_known_services) &&
           host_vocabulary(owner, weivel_json_object_get(given, "vocabulary"), &setup->vocabulary);
}

static int read_provider_script(const weivel_json_value *given, resolve_run *run) {
    const weivel_json_value *provider = weivel_json_object_get(given, "provider");
    const weivel_json_value *serves_only = weivel_json_object_get(provider, "servesOnlyUnconstrained");
    run->attributes_by_poll = weivel_json_object_get(provider, "attributesByPoll");
    run->serves_only_unconstrained = 0;
    return serves_only == NULL || weivel_json_as_bool(serves_only, &run->serves_only_unconstrained);
}

static int open_plugin(core_session *session, const weivel_manifest *manifest, weivel_handle *plugin) {
    weivel_status status = weivel_register_manifest(session->core, manifest, plugin, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    status = weivel_open_plugin(session->core, *plugin, NULL, NULL);
    session_drain(session);
    return status == WEIVEL_STATUS_OK;
}

static int start_want(resolve_run *run, const resolve_setup *setup) {
    weivel_handle provider = 0;
    weivel_handle consumer = 0;
    weivel_handle registration = 0;
    weivel_want_options options = WEIVEL_INIT(weivel_want_options);
    weivel_status status;
    if (!open_plugin(&run->session, &setup->provider, &provider) ||
        !open_plugin(&run->session, &setup->consumer, &consumer)) {
        return 0;
    }
    status = weivel_register_service(run->session.core, provider, setup->service_id, &registration, NULL);
    session_drain(&run->session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    options.now = run->now;
    options.budgeted = WEIVEL_CHOICE_YES;
    options.budget_millis = setup->budget_millis;
    options.poll_millis = setup->poll_millis;
    status = weivel_want_service(run->session.core, consumer, setup->service_id, &options, &run->want, NULL);
    session_drain(&run->session);
    return status == WEIVEL_STATUS_OK;
}

static void advance_until_settled(resolve_run *run, int64_t poll_millis) {
    uint32_t tick;
    int64_t deadline;
    for (tick = 0; tick < RESOLVE_TICK_LIMIT && !run->settled && !run->failed; tick++) {
        if (weivel_next_deadline(run->session.core, &deadline, NULL) != WEIVEL_STATUS_OK) {
            return;
        }
        run->now += poll_millis;
        if (weivel_advance(run->session.core, run->now, NULL) != WEIVEL_STATUS_OK) {
            run->failed = 1;
        }
        session_drain(&run->session);
    }
}

static void write_optional_text(answer_writer *answer, const char *text) {
    if (text == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, text);
    }
}

static void write_resolution(answer_writer *answer, const resolve_run *run) {
    int served = run->implementation != 0 && !run->degraded;
    uint32_t at;
    answer_begin_object(answer);
    answer_key(answer, "served");
    answer_bool(answer, served);
    answer_key(answer, "atPoll");
    if (served) {
        answer_int(answer, run->reads_at_service);
    } else {
        answer_null(answer);
    }
    answer_key(answer, "degraded");
    answer_bool(answer, run->degraded);
    answer_key(answer, "asks");
    answer_begin_array(answer);
    for (at = 0; at < run->ask_count; at++) {
        answer_bool(answer, run->asks[at]);
    }
    answer_end_array(answer);
    answer_key(answer, "report");
    write_optional_text(answer, run->report);
    answer_key(answer, "refusal");
    session_answer_refusal(answer, run->code, run->detail, run->fix);
    answer_end_object(answer);
}

static void release_run(resolve_run *run) {
    free(run->detail);
    free(run->fix);
    free(run->log_just_before);
    free(run->report);
}

static int drive(resolve_run *run, const resolve_setup *setup, answer_writer *answer) {
    weivel_core_options options = WEIVEL_INIT(weivel_core_options);
    int answered = 0;
    options.well_known_services = setup->well_known_services;
    options.constraint_vocabulary = setup->vocabulary;
    if (!session_open(&run->session, &options, read_event, run)) {
        return 0;
    }
    if (start_want(run, setup)) {
        advance_until_settled(run, setup->poll_millis);
        answered = run->settled && !run->failed && !run->session.failed;
    }
    if (answered) {
        write_resolution(answer, run);
    }
    session_close(&run->session);
    return answered;
}

int adapter_resolve(const weivel_json_value *given, answer_writer *answer) {
    arena owner;
    resolve_setup setup;
    resolve_run run = {0};
    int answered = 0;
    arena_init(&owner);
    if (read_setup(&owner, given, &setup) && read_provider_script(given, &run)) {
        answered = drive(&run, &setup, answer);
    }
    release_run(&run);
    arena_release(&owner);
    return answered;
}
