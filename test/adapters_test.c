#include "check.h"
#include "ops/adapters.h"
#include "ops/session.h"

/** The adapter's answer to `given_text`, parsed; NULL when it answered nothing or an incomplete document. */
static json_value *answer_of(op_adapter adapter, const char *given_text) {
    json_value *given = json_parse(given_text, strlen(given_text), NULL);
    json_value *parsed = NULL;
    answer_writer answer;
    answer_init(&answer);
    if (given != NULL && adapter(given, &answer) && answer_is_complete(&answer)) {
        parsed = json_parse(answer_text(&answer), strlen(answer_text(&answer)), NULL);
    }
    answer_release(&answer);
    json_free(given);
    return parsed;
}

static int answers_nothing(op_adapter adapter, const char *given_text) {
    json_value *given = json_parse(given_text, strlen(given_text), NULL);
    answer_writer answer;
    int answered;
    answer_init(&answer);
    answered = given != NULL && adapter(given, &answer);
    answer_release(&answer);
    json_free(given);
    return !answered;
}

static const char *text_at(const json_value *object, const char *key) {
    return json_as_string(json_object_get(object, key));
}

static int flag_at(const json_value *object, const char *key) {
    int flag = -1;
    json_as_bool(json_object_get(object, key), &flag);
    return flag;
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
    json_value *answer = answer_of(adapter_supports, "{\"host\":{\"applications\":[\"studio\"],\"api\":1},"
                                                     "\"facts\":{\"id\":\"thing\",\"api\":2,\"targets\":[\"studio\"]}}");
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "supported") == 0);
    check_text(text_at(answer, "axis"), "api", "axis");
    check_text(text_at(json_object_get(answer, "refusal"), "code"), "incompatible", "refusal code");
    CHECK(text_at(json_object_get(answer, "refusal"), "detail") != NULL);
    json_free(answer);
}

static void starts_a_provider_before_the_consumer_that_needs_it(void) {
    json_value *answer = answer_of(adapter_boot,
                                   "{\"units\":[{\"id\":\"consumer\",\"provides\":[],\"requires\":[{\"id\":\"store\","
                                   "\"optional\":false,\"degradable\":false}]},"
                                   "{\"id\":\"store\",\"provides\":[\"store\"],\"requires\":[]}]}");
    const json_value *started = json_object_get(answer, "started");
    CHECK(answer != NULL);
    CHECK(json_array_length(started) == 2);
    check_text(json_as_string(json_array_at(started, 0)), "store", "first started");
    check_text(json_as_string(json_array_at(started, 1)), "consumer", "second started");
    CHECK(json_kind_of(json_object_get(answer, "refusal")) == JSON_NULL);
    json_free(answer);
}

static void answers_each_edge_as_provider_then_consumer(void) {
    json_value *answer = answer_of(adapter_order,
                                   "{\"edgeSource\":\"capabilities\",\"units\":["
                                   "{\"id\":\"api\",\"provides\":[{\"id\":\"api\",\"kind\":\"capability\"}],"
                                   "\"requires\":[{\"id\":\"store\",\"kind\":\"capability\",\"optional\":false}]},"
                                   "{\"id\":\"store\",\"provides\":[{\"id\":\"store\",\"kind\":\"capability\"}],\"requires\":[]}]}");
    const json_value *edges = json_object_get(answer, "edges");
    CHECK(answer != NULL);
    CHECK(json_array_length(edges) == 1);
    check_text(json_as_string(json_array_at(json_array_at(edges, 0), 0)), "store", "edge provider");
    check_text(json_as_string(json_array_at(json_array_at(edges, 0), 1)), "api", "edge consumer");
    CHECK(json_kind_of(json_object_get(answer, "cycle")) == JSON_NULL);
    json_free(answer);
}

static void refuses_to_answer_a_requirement_of_an_unknown_kind(void) {
    CHECK(answers_nothing(adapter_order, "{\"edgeSource\":\"services\",\"units\":[{\"id\":\"a\",\"provides\":[],"
                                         "\"requires\":[{\"id\":\"b\",\"kind\":\"widget\",\"optional\":false}]}]}"));
    CHECK(answers_nothing(adapter_order, "{\"edgeSource\":\"everything\",\"units\":[]}"));
}

static void hands_the_object_form_of_a_consumed_service_to_the_core(void) {
    json_value *answer = answer_of(adapter_unbound,
                                   "{\"manifest\":{\"importName\":\"agenda\",\"api\":1,\"services\":{\"consumes\":["
                                   "\"mail:inbox\",{\"id\":\"calendar:events\",\"optional\":true},"
                                   "{\"id\":\"notes:pages\",\"provider\":\"forge:7\"},\"accounts\"]}}}");
    const json_value *services = json_object_get(answer, "services");
    CHECK(answer != NULL);
    CHECK(json_array_length(services) == 2);
    check_text(json_as_string(json_array_at(services, 0)), "mail:inbox", "first unbound");
    check_text(json_as_string(json_array_at(services, 1)), "calendar:events", "second unbound");
    json_free(answer);
}

static void reports_why_a_want_degraded_and_what_it_asked(void) {
    json_value *answer = answer_of(adapter_resolve,
                                   "{\"requirement\":{\"id\":\"store\",\"constraints\":{\"minReplicas\":3},"
                                   "\"degradable\":true},\"provider\":{\"advertised\":{\"maxReplicas\":1},"
                                   "\"attributesByPoll\":[{\"maxReplicas\":1}]},\"budgetMillis\":2000,\"pollMillis\":1000}");
    const json_value *asks = json_object_get(answer, "asks");
    const char *report = text_at(answer, "report");
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "degraded") == 1);
    CHECK(flag_at(answer, "served") == 0);
    CHECK(report != NULL && strlen(report) > 0);
    CHECK(json_array_length(asks) == 1);
    CHECK(json_kind_of(json_object_get(answer, "refusal")) == JSON_NULL);
    json_free(answer);
}

static void counts_the_poll_that_served(void) {
    json_value *answer = answer_of(adapter_resolve,
                                   "{\"requirement\":{\"id\":\"store\",\"constraints\":{\"maxLatencyMs\":50},"
                                   "\"degradable\":false},\"provider\":{\"advertised\":{\"latencyMs\":90},"
                                   "\"attributesByPoll\":[{},{},{\"latencyMs\":40}]},\"budgetMillis\":9000,\"pollMillis\":1000}");
    int64_t at_poll = 0;
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "served") == 1);
    CHECK(json_as_int64(json_object_get(answer, "atPoll"), &at_poll) && at_poll == 3);
    CHECK(json_kind_of(json_object_get(answer, "report")) == JSON_NULL);
    json_free(answer);
}

static void refuses_to_answer_a_resolve_without_a_poll_interval(void) {
    CHECK(answers_nothing(adapter_resolve, "{\"requirement\":{\"id\":\"store\",\"constraints\":{\"minReplicas\":1}},"
                                           "\"provider\":{\"advertised\":{}},\"budgetMillis\":10}"));
}

static void hands_back_the_payload_it_was_given(void) {
    json_value *answer = answer_of(adapter_carry, "{\"manifest\":{\"id\":\"thing\",\"api\":1}}");
    CHECK(answer != NULL);
    CHECK(flag_at(answer, "sameObject") == 1);
    json_free(answer);
    CHECK(answers_nothing(adapter_carry, "{}"));
}

int main(void) {
    names_every_refusal_code_as_the_header_spells_it();
    answers_the_api_axis_when_only_the_floor_refuses();
    starts_a_provider_before_the_consumer_that_needs_it();
    answers_each_edge_as_provider_then_consumer();
    refuses_to_answer_a_requirement_of_an_unknown_kind();
    hands_the_object_form_of_a_consumed_service_to_the_core();
    reports_why_a_want_degraded_and_what_it_asked();
    counts_the_poll_that_served();
    refuses_to_answer_a_resolve_without_a_poll_interval();
    hands_back_the_payload_it_was_given();
    return check_summary("adapters_test");
}
