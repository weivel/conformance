#include "adapters.h"
#include "facts.h"
#include "session.h"

#include <string.h>

typedef int (*unit_reader)(arena *owner, const json_value *unit, weivel_manifest *manifest);

/** The registered units of one vector, each handle at its unit's index. */
typedef struct {
    const json_value *units;
    weivel_handle *handles;
    uint32_t count;
} registered_units;

static int boot_unit(arena *owner, const json_value *unit, weivel_manifest *manifest) {
    weivel_manifest built = WEIVEL_INIT(weivel_manifest);
    *manifest = built;
    manifest->import_name = json_as_string(json_object_get(unit, "id"));
    return manifest->import_name != NULL &&
           facts_service_offers(owner, json_object_get(unit, "provides"), &manifest->services.provides) &&
           facts_service_wants(owner, json_object_get(unit, "requires"), &manifest->services.consumes);
}

static int is_kind(const json_value *entry, const char *kind) {
    const char *named = json_as_string(json_object_get(entry, "kind"));
    return named != NULL && strcmp(named, kind) == 0;
}

static int has_known_kinds(const json_value *array) {
    size_t at;
    if (array == NULL || json_kind_of(array) != JSON_ARRAY) {
        return 0;
    }
    for (at = 0; at < json_array_length(array); at++) {
        const json_value *entry = json_array_at(array, at);
        if (!is_kind(entry, "capability") && !is_kind(entry, "service")) {
            return 0;
        }
    }
    return 1;
}

static uint32_t count_of_kind(const json_value *array, const char *kind) {
    uint32_t count = 0;
    size_t at;
    for (at = 0; at < json_array_length(array); at++) {
        count += (uint32_t)is_kind(json_array_at(array, at), kind);
    }
    return count;
}

static int capability_ids(arena *owner, const json_value *array, weivel_strings *ids) {
    uint32_t count = count_of_kind(array, "capability");
    const char **items = arena_take(owner, count, sizeof *items);
    uint32_t filled = 0;
    size_t at;
    ids->items = items;
    ids->count = count;
    for (at = 0; at < json_array_length(array) && items != NULL; at++) {
        const json_value *entry = json_array_at(array, at);
        if (!is_kind(entry, "capability")) {
            continue;
        }
        items[filled] = json_as_string(json_object_get(entry, "id"));
        if (items[filled++] == NULL) {
            return 0;
        }
    }
    return count == 0 || items != NULL;
}

static int service_offers(arena *owner, const json_value *array, weivel_service_offers *offers) {
    uint32_t count = count_of_kind(array, "service");
    weivel_service_offer *items = arena_take(owner, count, sizeof *items);
    uint32_t filled = 0;
    size_t at;
    offers->items = items;
    offers->count = count;
    for (at = 0; at < json_array_length(array) && items != NULL; at++) {
        const json_value *entry = json_array_at(array, at);
        if (!is_kind(entry, "service")) {
            continue;
        }
        items[filled].size = (uint32_t)sizeof items[filled];
        items[filled].id = json_as_string(json_object_get(entry, "id"));
        if (items[filled++].id == NULL) {
            return 0;
        }
    }
    return count == 0 || items != NULL;
}

static int service_wants(arena *owner, const json_value *array, weivel_service_wants *wants) {
    uint32_t count = count_of_kind(array, "service");
    weivel_service_want *items = arena_take(owner, count, sizeof *items);
    uint32_t filled = 0;
    size_t at;
    wants->items = items;
    wants->count = count;
    for (at = 0; at < json_array_length(array) && items != NULL; at++) {
        const json_value *entry = json_array_at(array, at);
        weivel_service_want *want;
        if (!is_kind(entry, "service")) {
            continue;
        }
        want = &items[filled++];
        want->size = (uint32_t)sizeof *want;
        want->id = json_as_string(json_object_get(entry, "id"));
        if (want->id == NULL || !facts_choice(json_object_get(entry, "optional"), &want->optional)) {
            return 0;
        }
    }
    return count == 0 || items != NULL;
}

/**
 * @implNote `optional` on a capability requirement is read by nothing: the core holds a consumed capability
 * optional always, so the adapter hands it only the id, exactly as a manifest declares one.
 */
static int order_unit(arena *owner, const json_value *unit, weivel_manifest *manifest) {
    const json_value *provides = json_object_get(unit, "provides");
    const json_value *requires = json_object_get(unit, "requires");
    weivel_manifest built = WEIVEL_INIT(weivel_manifest);
    *manifest = built;
    manifest->import_name = json_as_string(json_object_get(unit, "id"));
    return manifest->import_name != NULL && has_known_kinds(provides) && has_known_kinds(requires) &&
           capability_ids(owner, provides, &manifest->capabilities.provides) &&
           capability_ids(owner, requires, &manifest->capabilities.consumes) &&
           service_offers(owner, provides, &manifest->services.provides) &&
           service_wants(owner, requires, &manifest->services.consumes);
}

static int register_unit(core_session *session, unit_reader reader, const json_value *unit, weivel_handle *plugin) {
    arena owner;
    weivel_manifest manifest;
    int registered = 0;
    arena_init(&owner);
    if (reader(&owner, unit, &manifest)) {
        registered = weivel_register_manifest(session->core, &manifest, plugin, NULL) == WEIVEL_STATUS_OK;
        session_drain(session);
    }
    arena_release(&owner);
    return registered;
}

static int register_units(core_session *session, arena *owner, unit_reader reader, const json_value *units,
                          registered_units *registered) {
    uint32_t at;
    registered->units = units;
    registered->count = (uint32_t)json_array_length(units);
    registered->handles = arena_take(owner, registered->count, sizeof *registered->handles);
    if (units == NULL || json_kind_of(units) != JSON_ARRAY || (registered->count > 0 && registered->handles == NULL)) {
        return 0;
    }
    for (at = 0; at < registered->count; at++) {
        if (!register_unit(session, reader, json_array_at(units, at), &registered->handles[at])) {
            return 0;
        }
    }
    return 1;
}

static const char *unit_id(const registered_units *registered, weivel_handle plugin) {
    uint32_t at;
    for (at = 0; at < registered->count; at++) {
        if (registered->handles[at] == plugin) {
            return json_as_string(json_object_get(json_array_at(registered->units, at), "id"));
        }
    }
    return NULL;
}

static int write_ids(answer_writer *answer, const registered_units *registered, const weivel_handle *plugins,
                     uint32_t count) {
    uint32_t at;
    answer_begin_array(answer);
    for (at = 0; at < count; at++) {
        const char *id = unit_id(registered, plugins[at]);
        if (id == NULL) {
            return 0;
        }
        answer_string(answer, id);
    }
    answer_end_array(answer);
    return 1;
}

static void write_first_refusal(answer_writer *answer, const weivel_activation_plan *plan) {
    answer_key(answer, "refusal");
    if (plan->refusal_count == 0) {
        answer_null(answer);
        return;
    }
    session_answer_refusal(answer, plan->refusals[0].refusal.code, plan->refusals[0].refusal.detail,
                           plan->refusals[0].refusal.fix);
}

static int write_boot(answer_writer *answer, const registered_units *registered, const weivel_activation_plan *plan) {
    int written;
    answer_begin_object(answer);
    answer_key(answer, "started");
    written = write_ids(answer, registered, plan->order, plan->order_count);
    write_first_refusal(answer, plan);
    answer_end_object(answer);
    return written;
}

static int write_edges(answer_writer *answer, const registered_units *registered, const weivel_activation_plan *plan) {
    uint32_t at;
    answer_begin_array(answer);
    for (at = 0; at < plan->edge_count; at++) {
        weivel_handle pair[2];
        pair[0] = plan->edges[at].provider;
        pair[1] = plan->edges[at].consumer;
        if (!write_ids(answer, registered, pair, 2)) {
            return 0;
        }
    }
    answer_end_array(answer);
    return 1;
}

static int write_order(answer_writer *answer, const registered_units *registered, const weivel_activation_plan *plan) {
    int written;
    answer_begin_object(answer);
    answer_key(answer, "order");
    written = write_ids(answer, registered, plan->order, plan->order_count);
    answer_key(answer, "edges");
    written = written && write_edges(answer, registered, plan);
    answer_key(answer, "cycle");
    if (plan->cycle_count == 0) {
        answer_null(answer);
    } else {
        written = written && write_ids(answer, registered, plan->cycles[0].members, plan->cycles[0].count);
    }
    write_first_refusal(answer, plan);
    answer_end_object(answer);
    return written;
}

typedef int (*plan_writer)(answer_writer *answer, const registered_units *registered,
                           const weivel_activation_plan *plan);

/** The parameters of one activation plan an adapter asks the core for. */
typedef struct {
    unit_reader reader;
    uint32_t edge_source;
    const json_value *host_provided;
    plan_writer writer;
} plan_question;

static int ask_plan(core_session *session, arena *owner, const json_value *units, const plan_question *question,
                    answer_writer *answer) {
    registered_units registered;
    weivel_strings host_provided;
    weivel_activation_plan plan = WEIVEL_INIT(weivel_activation_plan);
    weivel_status status;
    int written;
    if (!facts_strings(owner, question->host_provided, &host_provided) ||
        !register_units(session, owner, question->reader, units, &registered)) {
        return 0;
    }
    status = weivel_plan_activation(session->core, registered.handles, registered.count, question->edge_source,
                                    &host_provided, &plan, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    written = question->writer(answer, &registered, &plan);
    weivel_activation_plan_release(&plan);
    return written && !session->failed;
}

static int answer_plan(const json_value *units, const plan_question *question, answer_writer *answer) {
    arena owner;
    core_session session;
    int answered = 0;
    arena_init(&owner);
    if (session_open(&session, NULL, NULL, NULL)) {
        answered = ask_plan(&session, &owner, units, question, answer);
        session_close(&session);
    }
    arena_release(&owner);
    return answered;
}

/**
 * @implNote A boot requirement may be mandatory, and a consumed capability never is, so the units are planned by
 * their services.
 */
int adapter_boot(const json_value *given, answer_writer *answer) {
    plan_question question = {boot_unit, WEIVEL_EDGES_SERVICES, NULL, write_boot};
    return answer_plan(json_object_get(given, "units"), &question, answer);
}

static int edge_source_of(const json_value *named, uint32_t *edge_source) {
    const char *source = json_as_string(named);
    if (source != NULL && strcmp(source, "capabilities") == 0) {
        *edge_source = WEIVEL_EDGES_CAPABILITIES;
        return 1;
    }
    if (source != NULL && strcmp(source, "services") == 0) {
        *edge_source = WEIVEL_EDGES_SERVICES;
        return 1;
    }
    return 0;
}

int adapter_order(const json_value *given, answer_writer *answer) {
    plan_question question = {order_unit, 0, json_object_get(given, "hostProvided"), write_order};
    if (!edge_source_of(json_object_get(given, "edgeSource"), &question.edge_source)) {
        return 0;
    }
    return answer_plan(json_object_get(given, "units"), &question, answer);
}
