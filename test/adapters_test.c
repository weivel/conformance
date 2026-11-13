#include "check.h"
#include "ops/adapters.h"
#include "ops/session.h"
#include "ops/standard_refusal.h"
#include "ops/want_states.h"

#include <weivel/core.h>

/** The adapter's answer to `given_text`, parsed; NULL when it answered nothing or an incomplete document. */
static weivel_json_value *answer_of(op_adapter adapter, const char *given_text) {
    weivel_json_value *given = weivel_json_parse(given_text, strlen(given_text), NULL);
    weivel_json_value *parsed = NULL;
    answer_writer answer;
    answer_init(&answer);
    if (given != NULL && adapter(given, &answer) && answer_is_complete(&answer)) {
        parsed = weivel_json_parse(answer_text(&answer), strlen(answer_text(&answer)), NULL);
    }
    answer_release(&answer);
    weivel_json_free(given);
    return parsed;
}

static int answers_nothing(op_adapter adapter, const char *given_text) {
    weivel_json_value *given = weivel_json_parse(given_text, strlen(given_text), NULL);
    answer_writer answer;
    int answered;
    answer_init(&answer);
    answered = given != NULL && adapter(given, &answer);
    answer_release(&answer);
    weivel_json_free(given);
    return !answered;
}

#define HOST_SERVICES "\"wellKnownServices\":[\"store\"],"
#define HOST_VOCABULARY_LOWER "{\"constraint\":\"minReplicas\",\"attribute\":\"maxReplicas\",\"bound\":\"lower\"},"
#define HOST_VOCABULARY_UPPER "{\"constraint\":\"maxLatencyMs\",\"attribute\":\"latencyMs\",\"bound\":\"upper\"}"
#define HOST_VOCABULARY "\"vocabulary\":[" HOST_VOCABULARY_LOWER HOST_VOCABULARY_UPPER "],"
#define NEVER_REQUIREMENT "\"requirement\":{\"id\":\"store\",\"constraints\":{\"minReplicas\":3},\"degradable\":true},"
#define NEVER_PROVIDER "\"provider\":{\"advertised\":{\"maxReplicas\":1},\"attributesByPoll\":[{\"maxReplicas\":1}]},"
#define NEVER_SATISFIED NEVER_REQUIREMENT NEVER_PROVIDER

static const char *text_at(const weivel_json_value *object, const char *key) {
    return weivel_json_as_string(weivel_json_object_get(object, key));
}

/** 1 when `key` is present and null; a missing answer or member is not null, so the check fails rather than crashes. */
static int is_null_at(const weivel_json_value *object, const char *key) {
    const weivel_json_value *member = weivel_json_object_get(object, key);
    return member != NULL && weivel_json_kind_of(member) == WEIVEL_JSON_NULL;
}

static int flag_at(const weivel_json_value *object, const char *key) {
    int flag = -1;
    weivel_json_as_bool(weivel_json_object_get(object, key), &flag);
    return flag;
}

static const weivel_json_value *refusal_of(const weivel_json_value *answer) {
    return weivel_json_object_get(answer, "refusal");
}

static void names_every_refusal_code_as_the_header_spells_it(void) {
    check_text(session_refusal_name(WEIVEL_REFUSAL_MALFORMED_CALL), "malformed_call", "malformed call");
    check_text(session_refusal_name(WEIVEL_REFUSAL_INCOMPATIBLE), "incompatible", "incompatible");
    check_text(session_refusal_name(WEIVEL_REFUSAL_UNMET_NEED), "unmet_need", "unmet need");
    check_text(session_refusal_name(WEIVEL_REFUSAL_OUTSIDE_NAMESPACE), "outside_namespace", "outside namespace");
    check_text(session_refusal_name(WEIVEL_REFUSAL_UNMET_CONSTRAINT), "unmet_constraint", "unmet constraint");
    check_text(session_refusal_name(WEIVEL_REFUSAL_NOT_SERVED), "not_served", "not served");
    check_text(session_refusal_name(WEIVEL_REFUSAL_PROVIDED_TWICE), "provided_twice", "provided twice");
    CHECK(session_refusal_name(WEIVEL_REFUSAL_PROVIDED_TWICE + 1) == NULL);
}

static void answers_the_api_axis_when_only_the_floor_refuses(void) {
    weivel_json_value *answer = answer_of(adapter_supports, "{\"host\":{\"applications\":[\"studio\"],\"api\":1},"
                                                     "\"facts\":{\"id\":\"thing\",\"api\":2,\"targets\":[\"studio\"]}}");
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "supported") == 0);
    check_text(text_at(answer, "axis"), "api", "axis");
    check_text(text_at(weivel_json_object_get(answer, "refusal"), "code"), "incompatible", "refusal code");
    CHECK(text_at(weivel_json_object_get(answer, "refusal"), "detail") != NULL);
    check_text(text_at(refusal_of(answer), "subject"), "thing", "incompatible subject");
    CHECK(flag_at(refusal_of(answer), "hasFix") == 1);
    weivel_json_free(answer);
}

static void names_the_need_a_refused_boot_lacks(void) {
    weivel_json_value *answer = answer_of(adapter_boot, "{\"units\":[{\"id\":\"consumer\",\"provides\":[],"
                                                        "\"requires\":[{\"id\":\"store\",\"optional\":false,"
                                                        "\"degradable\":false}]}]}");
    check_text(text_at(refusal_of(answer), "code"), "unmet_need", "boot code");
    check_text(text_at(refusal_of(answer), "subject"), "store", "boot subject");
    CHECK(flag_at(refusal_of(answer), "hasFix") == 1);
    weivel_json_free(answer);
}

static void names_the_id_an_undeclared_reach_was_for(void) {
    weivel_json_value *answer = answer_of(adapter_reach, "{\"plugins\":[{\"id\":\"notes\",\"consumes\":[]}],"
                                                         "\"steps\":[{\"plugin\":\"notes\",\"get\":\"clock\"}]}");
    check_text(text_at(refusal_of(answer), "code"), "undeclared", "reach code");
    check_text(text_at(refusal_of(answer), "subject"), "clock", "reach subject");
    CHECK(flag_at(refusal_of(answer), "hasFix") == 1);
    weivel_json_free(answer);
}

static void names_the_constraint_a_refused_want_missed(void) {
    weivel_json_value *answer = answer_of(adapter_resolve,
                                          "{" HOST_SERVICES HOST_VOCABULARY
                                          "\"requirement\":{\"id\":\"store\",\"constraints\":{\"minReplicas\":3},"
                                          "\"degradable\":false}," NEVER_PROVIDER
                                          "\"budgetMillis\":2000,\"pollMillis\":1000}");
    check_text(text_at(refusal_of(answer), "code"), "unmet_constraint", "resolve code");
    check_text(text_at(refusal_of(answer), "subject"), "minReplicas", "resolve subject");
    CHECK(flag_at(refusal_of(answer), "hasFix") == 1);
    weivel_json_free(answer);
}

static void starts_a_provider_before_the_consumer_that_needs_it(void) {
    weivel_json_value *answer = answer_of(adapter_boot,
                                   "{\"units\":[{\"id\":\"consumer\",\"provides\":[],\"requires\":[{\"id\":\"store\","
                                   "\"optional\":false,\"degradable\":false}]},"
                                   "{\"id\":\"store\",\"provides\":[\"store\"],\"requires\":[]}]}");
    const weivel_json_value *started = weivel_json_object_get(answer, "started");
    CHECK(answer != NULL);
    CHECK(weivel_json_array_length(started) == 2);
    check_text(weivel_json_as_string(weivel_json_array_at(started, 0)), "store", "first started");
    check_text(weivel_json_as_string(weivel_json_array_at(started, 1)), "consumer", "second started");
    CHECK(is_null_at(answer, "refusal"));
    weivel_json_free(answer);
}

static void answers_each_edge_as_provider_then_consumer(void) {
    weivel_json_value *answer = answer_of(adapter_order,
                                   "{\"edgeSource\":\"capabilities\",\"units\":["
                                   "{\"id\":\"api\",\"provides\":[{\"id\":\"api\",\"kind\":\"capability\"}],"
                                   "\"requires\":[{\"id\":\"store\",\"kind\":\"capability\",\"optional\":false}]},"
                                   "{\"id\":\"store\",\"provides\":[{\"id\":\"store\",\"kind\":\"capability\"}],\"requires\":[]}]}");
    const weivel_json_value *edges = weivel_json_object_get(answer, "edges");
    CHECK(answer != NULL);
    CHECK(weivel_json_array_length(edges) == 1);
    check_text(weivel_json_as_string(weivel_json_array_at(weivel_json_array_at(edges, 0), 0)), "store", "edge provider");
    check_text(weivel_json_as_string(weivel_json_array_at(weivel_json_array_at(edges, 0), 1)), "api", "edge consumer");
    CHECK(is_null_at(answer, "cycle"));
    weivel_json_free(answer);
}

static void refuses_to_answer_a_requirement_of_an_unknown_kind(void) {
    CHECK(answers_nothing(adapter_order, "{\"edgeSource\":\"services\",\"units\":[{\"id\":\"a\",\"provides\":[],"
                                         "\"requires\":[{\"id\":\"b\",\"kind\":\"widget\",\"optional\":false}]}]}"));
    CHECK(answers_nothing(adapter_order, "{\"edgeSource\":\"everything\",\"units\":[]}"));
}

static void refuses_to_answer_a_unit_whose_lists_are_not_arrays(void) {
    CHECK(answers_nothing(adapter_order, "{\"edgeSource\":\"services\",\"units\":[{\"id\":\"a\",\"provides\":{},"
                                         "\"requires\":[]}]}"));
    CHECK(answers_nothing(adapter_order, "{\"edgeSource\":\"services\",\"units\":[{\"id\":\"a\",\"provides\":[]}]}"));
}

static void hands_the_object_form_of_a_consumed_service_to_the_core(void) {
    weivel_json_value *answer = answer_of(adapter_unbound,
                                   "{\"manifest\":{\"importName\":\"agenda\",\"api\":1,\"services\":{\"consumes\":["
                                   "\"mail:inbox\",{\"id\":\"calendar:events\",\"optional\":true},"
                                   "{\"id\":\"notes:pages\",\"provider\":\"forge:7\"},\"accounts\"]}}}");
    const weivel_json_value *services = weivel_json_object_get(answer, "services");
    CHECK(answer != NULL);
    CHECK(weivel_json_array_length(services) == 2);
    check_text(weivel_json_as_string(weivel_json_array_at(services, 0)), "mail:inbox", "first unbound");
    check_text(weivel_json_as_string(weivel_json_array_at(services, 1)), "calendar:events", "second unbound");
    weivel_json_free(answer);
}

static void reports_why_a_want_degraded_and_what_it_asked(void) {
    weivel_json_value *answer = answer_of(adapter_resolve, "{" HOST_SERVICES HOST_VOCABULARY NEVER_SATISFIED
                                                    "\"budgetMillis\":2000,\"pollMillis\":1000}");
    const weivel_json_value *asks = weivel_json_object_get(answer, "asks");
    const weivel_json_value *report = weivel_json_object_get(answer, "report");
    const char *text = text_at(report, "text");
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "degraded") == 1);
    CHECK(flag_at(answer, "served") == 0);
    CHECK(text != NULL && strlen(text) > 0);
    check_text(text_at(report, "subject"), "minReplicas", "degrade subject");
    CHECK(flag_at(report, "hasFix") == 0);
    CHECK(weivel_json_array_length(asks) == 1);
    CHECK(is_null_at(answer, "atPoll"));
    CHECK(is_null_at(answer, "refusal"));
    weivel_json_free(answer);
}

static void counts_the_poll_that_served(void) {
    weivel_json_value *answer = answer_of(adapter_resolve,
                                   "{" HOST_SERVICES HOST_VOCABULARY
                                   "\"requirement\":{\"id\":\"store\",\"constraints\":{\"maxLatencyMs\":50},"
                                   "\"degradable\":false},\"provider\":{\"advertised\":{\"latencyMs\":90},"
                                   "\"attributesByPoll\":[{},{},{\"latencyMs\":40}]},\"budgetMillis\":9000,\"pollMillis\":1000}");
    int64_t at_poll = 0;
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "served") == 1);
    CHECK(weivel_json_as_int64(weivel_json_object_get(answer, "atPoll"), &at_poll) && at_poll == 3);
    CHECK(is_null_at(answer, "report"));
    weivel_json_free(answer);
}

static void refuses_to_answer_a_resolve_missing_a_premise(void) {
    CHECK(answers_nothing(adapter_resolve, "{" HOST_SERVICES HOST_VOCABULARY NEVER_SATISFIED "\"budgetMillis\":10}"));
    CHECK(answers_nothing(adapter_resolve,
                          "{" HOST_VOCABULARY NEVER_SATISFIED "\"budgetMillis\":2000,\"pollMillis\":1000}"));
    CHECK(answers_nothing(adapter_resolve,
                          "{" HOST_SERVICES NEVER_SATISFIED "\"budgetMillis\":2000,\"pollMillis\":1000}"));
    CHECK(answers_nothing(adapter_resolve, "{" HOST_SERVICES
                                           "\"vocabulary\":[{\"constraint\":\"minReplicas\",\"attribute\":\"maxReplicas\","
                                           "\"bound\":\"sideways\"}]," NEVER_SATISFIED
                                           "\"budgetMillis\":2000,\"pollMillis\":1000}"));
}

static void refuses_a_resolve_without_well_known_services_even_when_the_id_needs_none(void) {
    CHECK(answers_nothing(adapter_resolve, "{" HOST_VOCABULARY "\"requirement\":{\"id\":\"provider:store\",\"constraints\":{}},"
                                           "\"provider\":{\"advertised\":{}},\"budgetMillis\":10,\"pollMillis\":1}"));
}

static void answers_nothing_for_a_want_that_never_settled(void) {
    CHECK(answers_nothing(adapter_resolve, "{" HOST_SERVICES HOST_VOCABULARY NEVER_SATISFIED
                                           "\"budgetMillis\":1000000000,\"pollMillis\":1}"));
}

static void hands_back_the_payload_it_was_given(void) {
    weivel_json_value *answer = answer_of(adapter_carry, "{\"manifest\":{\"id\":\"thing\",\"api\":1}}");
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "sameObject") == 1);
    weivel_json_free(answer);
    CHECK(answers_nothing(adapter_carry, "{}"));
}

static void answers_a_want_nothing_settles_as_pending(void) {
    weivel_json_value *answer =
        answer_of(adapter_scope, "{\"steps\":[{\"consumer\":\"a\",\"want\":\"b:z\"},{\"lookup\":\"b:z\"}]}");
    CHECK(answer != NULL);
    check_text(weivel_json_as_string(weivel_json_array_at(weivel_json_object_get(answer, "wants"), 0)), "pending", "want state");
    weivel_json_free(answer);
}

static void ignore_event(const weivel_event *event, void *context) {
    (void)event;
    (void)context;
}

/** Reading that a want waits must leave it waiting, so a provider arriving afterwards still settles it. */
static void reads_a_waiting_want_without_ending_it(void) {
    core_session session;
    weivel_core_options options = WEIVEL_INIT(weivel_core_options);
    const char *store = "store";
    weivel_manifest manifests[2] = {WEIVEL_INIT(weivel_manifest), WEIVEL_INIT(weivel_manifest)};
    weivel_service_offer offer = WEIVEL_INIT(weivel_service_offer);
    weivel_service_want need = WEIVEL_INIT(weivel_service_want);
    weivel_handle plugins[2] = {0, 0};
    weivel_handle want = 0;
    weivel_handle registration = 0;
    uint32_t condition = 0;
    size_t at;
    offer.id = store;
    need.id = store;
    manifests[0].import_name = "provider";
    manifests[0].services.provides.items = &offer;
    manifests[0].services.provides.count = 1;
    manifests[1].import_name = "consumer";
    manifests[1].services.consumes.items = &need;
    manifests[1].services.consumes.count = 1;
    options.well_known_services.items = &store;
    options.well_known_services.count = 1;
    CHECK(session_open(&session, &options, ignore_event, NULL));
    for (at = 0; at < 2; at++) {
        CHECK(weivel_register_manifest(session.core, &manifests[at], &plugins[at], NULL) == WEIVEL_STATUS_OK);
        CHECK(weivel_open_plugin(session.core, plugins[at], NULL, NULL) == WEIVEL_STATUS_OK);
    }
    CHECK(weivel_want_service(session.core, plugins[1], store, NULL, &want, NULL) == WEIVEL_STATUS_OK);
    CHECK(session_still_live(&session, want));
    CHECK(session_still_live(&session, want));
    CHECK(weivel_register_service(session.core, plugins[0], store, &registration, NULL) == WEIVEL_STATUS_OK);
    CHECK(weivel_handle_state(session.core, want, &condition, NULL) == WEIVEL_STATUS_OK);
    CHECK(condition == WEIVEL_HANDLE_SETTLED);
    CHECK(!session_still_live(&session, want));
    CHECK(!session_still_live(&session, registration));
    session_close(&session);
}

static void read_one(hearing *log, uint32_t kind, uint32_t code, const char *subject, uint32_t has_fix) {
    weivel_event event = WEIVEL_INIT(weivel_event);
    event.kind = kind;
    if (kind == WEIVEL_EVENT_WANT_SETTLED) {
        event.data.want.refusal.code = code;
        event.data.want.refusal.detail = "what happened";
        event.data.want.refusal.fix = has_fix ? "what would fix it" : "";
        event.data.want.subject = subject;
        event.data.want.has_fix = has_fix;
    } else {
        event.data.refused.refusal.code = code;
        event.data.refused.refusal.detail = "what happened";
        event.data.refused.refusal.fix = has_fix ? "what would fix it" : "";
        event.data.refused.subject = subject;
        event.data.refused.has_fix = has_fix;
    }
    hearing_read(&event, log);
}

static void keeps_the_subject_of_the_first_refusal_whatever_carries_it(void) {
    hearing log;
    hearing_init(&log);
    read_one(&log, WEIVEL_EVENT_REFUSAL, WEIVEL_REFUSAL_SERVICE_TAKEN, "calendar:events", 1);
    read_one(&log, WEIVEL_EVENT_REFUSAL, WEIVEL_REFUSAL_OUTSIDE_NAMESPACE, "other", 0);
    check_text(log.first_subject, "calendar:events", "refusal event subject");
    CHECK(log.first_has_fix == 1);
    hearing_release(&log);
    hearing_init(&log);
    read_one(&log, WEIVEL_EVENT_WANT_SETTLED, WEIVEL_REFUSAL_TIMED_OUT, "store", 0);
    check_text(log.first_subject, "store", "want settlement subject");
    CHECK(log.first_has_fix == 0);
    hearing_release(&log);
}

static void answers_nothing_for_a_step_naming_no_plugin(void) {
    CHECK(answers_nothing(adapter_reach, "{\"plugins\":[{\"id\":\"notes\",\"consumes\":[\"clock\"]}],"
                                         "\"steps\":[{\"plugin\":\"stranger\",\"get\":\"clock\"}]}"));
}

static heard_event rejection_carrying(uint32_t code) {
    heard_event settlement;
    memset(&settlement, 0, sizeof settlement);
    settlement.kind = WEIVEL_EVENT_WANT_SETTLED;
    settlement.code = code;
    return settlement;
}

static void reads_a_rejection_only_for_the_codes_the_contract_means(void) {
    heard_event stopped = rejection_carrying(WEIVEL_REFUSAL_STOPPED);
    heard_event undeclared = rejection_carrying(WEIVEL_REFUSAL_UNDECLARED);
    heard_event wrong_provider = rejection_carrying(WEIVEL_REFUSAL_WRONG_PROVIDER);
    heard_event timed_out = rejection_carrying(WEIVEL_REFUSAL_TIMED_OUT);
    heard_event served = rejection_carrying(WEIVEL_REFUSAL_NONE);
    served.implementation = 7;
    check_text(want_state_in_scope(&stopped), "rejected", "scope want released");
    CHECK(want_state_in_scope(&undeclared) == NULL);
    CHECK(want_state_in_scope(&timed_out) == NULL);
    check_text(want_state_in_scope(&served), "settled", "scope want served");
    check_text(want_state_in_reach(&undeclared), "refused", "reach want undeclared");
    check_text(want_state_in_reach(&wrong_provider), "refused", "reach want from the wrong provider");
    CHECK(want_state_in_reach(&stopped) == NULL);
    CHECK(want_state_in_reach(&timed_out) == NULL);
    check_text(want_state_in_reach(&served), "served", "reach want served");
}

#define BARE_ROUTING "\"manifest\":{\"importName\":\"a\",\"api\":2,\"services\":{\"provides\":[\"routing\"]}}"

static void names_every_standard_refusal_code_as_the_header_spells_it(void) {
    check_text(standard_refusal_name(WEIVEL_STANDARD_REFUSAL_WRONG_TYPE), "wrong_type", "wrong type");
    check_text(standard_refusal_name(WEIVEL_STANDARD_REFUSAL_MISSING_FIELD), "missing_field", "missing field");
    check_text(standard_refusal_name(WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN), "not_well_known", "not well known");
    check_text(standard_refusal_name(WEIVEL_STANDARD_REFUSAL_NAME_MISMATCH), "name_mismatch", "name mismatch");
    CHECK(standard_refusal_name(WEIVEL_STANDARD_REFUSAL_LAST + 1) == NULL);
}

static void names_every_standard_refusal_code_up_to_the_highest(void) {
    uint32_t code;
    CHECK(standard_refusal_named_codes() == WEIVEL_STANDARD_REFUSAL_LAST + 1);
    for (code = 0; code < standard_refusal_named_codes(); code++) {
        if (standard_refusal_name(code) == NULL) {
            fprintf(stderr, "standard refusal code %u has no name\n", (unsigned)code);
            CHECK(!"every standard refusal code is named");
        }
    }
}

static int answers_refusal(const char *detail, const char *fix) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    answer_writer answer;
    int answered;
    refusal.code = WEIVEL_STANDARD_REFUSAL_INVALID_VALUE;
    refusal.detail = detail;
    refusal.fix = fix;
    refusal.path = "importName";
    answer_init(&answer);
    answered = standard_answer_refusal(&answer, &refusal);
    answer_release(&answer);
    return answered;
}

static void answers_nothing_for_a_refusal_missing_its_detail_or_fix(void) {
    CHECK(answers_refusal("importName is not a valid import name", "use lowercase words"));
    CHECK(!answers_refusal("", "use lowercase words"));
    CHECK(!answers_refusal("importName is not a valid import name", ""));
    CHECK(!answers_refusal(NULL, "use lowercase words"));
    CHECK(!answers_refusal("importName is not a valid import name", NULL));
}

static void answers_nothing_for_a_schema_other_than_the_published_one(void) {
    CHECK(answers_nothing(adapter_validate, "{\"schema\":\"library.schema.json\"," BARE_ROUTING "}"));
    CHECK(answers_nothing(adapter_validate, "{" BARE_ROUTING "}"));
    CHECK(answers_nothing(adapter_validate, "{\"schema\":\"plugin.schema.json\"}"));
}

static void skips_the_well_known_rule_only_when_the_list_is_absent(void) {
    weivel_json_value *absent = answer_of(adapter_validate, "{\"schema\":\"plugin.schema.json\"," BARE_ROUTING "}");
    weivel_json_value *empty =
        answer_of(adapter_validate, "{\"schema\":\"plugin.schema.json\",\"wellKnownServices\":[]," BARE_ROUTING "}");
    CHECK(flag_at(absent, "valid") == 1);
    CHECK(is_null_at(absent, "refusal"));
    CHECK(flag_at(empty, "valid") == 0);
    check_text(text_at(weivel_json_object_get(empty, "refusal"), "code"), "not_well_known", "empty list code");
    check_text(text_at(weivel_json_object_get(empty, "refusal"), "path"), "services.provides[0]", "empty list path");
    weivel_json_free(absent);
    weivel_json_free(empty);
}

static void hands_the_validator_a_string_with_an_escaped_newline_intact(void) {
    weivel_json_value *answer = answer_of(adapter_validate,
                                          "{\"schema\":\"plugin.schema.json\",\"manifest\":{\"importName\":\"a\\n\","
                                          "\"api\":2}}");
    CHECK(flag_at(answer, "valid") == 0);
    check_text(text_at(weivel_json_object_get(answer, "refusal"), "code"), "invalid_value", "escaped newline code");
    check_text(text_at(weivel_json_object_get(answer, "refusal"), "path"), "importName", "escaped newline path");
    weivel_json_free(answer);
}

static void answers_whether_a_value_is_an_identity(void) {
    weivel_json_value *forge = answer_of(adapter_identity, "{\"value\":\"forge:12/a\"}");
    weivel_json_value *bare = answer_of(adapter_identity, "{\"value\":\"a\"}");
    CHECK(flag_at(forge, "valid") == 1);
    CHECK(flag_at(bare, "valid") == 0);
    CHECK(answers_nothing(adapter_identity, "{\"value\":12}"));
    CHECK(answers_nothing(adapter_identity, "{}"));
    weivel_json_free(forge);
    weivel_json_free(bare);
}

int main(void) {
    names_every_refusal_code_as_the_header_spells_it();
    answers_the_api_axis_when_only_the_floor_refuses();
    names_the_need_a_refused_boot_lacks();
    names_the_id_an_undeclared_reach_was_for();
    names_the_constraint_a_refused_want_missed();
    starts_a_provider_before_the_consumer_that_needs_it();
    answers_each_edge_as_provider_then_consumer();
    refuses_to_answer_a_requirement_of_an_unknown_kind();
    refuses_to_answer_a_unit_whose_lists_are_not_arrays();
    hands_the_object_form_of_a_consumed_service_to_the_core();
    reports_why_a_want_degraded_and_what_it_asked();
    counts_the_poll_that_served();
    refuses_to_answer_a_resolve_missing_a_premise();
    refuses_a_resolve_without_well_known_services_even_when_the_id_needs_none();
    answers_nothing_for_a_want_that_never_settled();
    hands_back_the_payload_it_was_given();
    answers_a_want_nothing_settles_as_pending();
    reads_a_waiting_want_without_ending_it();
    keeps_the_subject_of_the_first_refusal_whatever_carries_it();
    answers_nothing_for_a_step_naming_no_plugin();
    reads_a_rejection_only_for_the_codes_the_contract_means();
    names_every_standard_refusal_code_as_the_header_spells_it();
    names_every_standard_refusal_code_up_to_the_highest();
    answers_nothing_for_a_refusal_missing_its_detail_or_fix();
    answers_nothing_for_a_schema_other_than_the_published_one();
    skips_the_well_known_rule_only_when_the_list_is_absent();
    hands_the_validator_a_string_with_an_escaped_newline_intact();
    answers_whether_a_value_is_an_identity();
    return check_summary("adapters_test");
}
