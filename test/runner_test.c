#include "check.h"
#include "report.h"
#include "runner.h"

#include <stdlib.h>

static vector make_vector(const char *then_text, vector_tier tier) {
    static const char GIVEN[] = "{}";
    vector candidate;
    weivel_json_value *document;
    char text[512];
    snprintf(text, sizeof text, "{\"id\":\"supports/example\",\"op\":\"supports\",\"covers\":\"12.1\",\"given\":%s,\"then\":%s}",
             GIVEN, then_text);
    document = weivel_json_parse(text, strlen(text), NULL);
    memset(&candidate, 0, sizeof candidate);
    candidate.document = document;
    candidate.id = weivel_json_as_string(weivel_json_object_get(document, "id"));
    candidate.op = weivel_json_as_string(weivel_json_object_get(document, "op"));
    candidate.covers = weivel_json_as_string(weivel_json_object_get(document, "covers"));
    candidate.given = weivel_json_object_get(document, "given");
    candidate.then = weivel_json_object_get(document, "then");
    candidate.tier = tier;
    return candidate;
}

static int answers_supported(const weivel_json_value *given, answer_writer *answer) {
    (void)given;
    answer_begin_object(answer);
    answer_key(answer, "supported");
    answer_bool(answer, 1);
    answer_end_object(answer);
    return 1;
}

static int answers_nothing(const weivel_json_value *given, answer_writer *answer) {
    (void)given;
    (void)answer;
    return 0;
}

static int answers_half_a_document(const weivel_json_value *given, answer_writer *answer) {
    (void)given;
    answer_begin_object(answer);
    return 1;
}

static int answers_a_valid_document_after_a_misuse(const weivel_json_value *given, answer_writer *answer) {
    answers_supported(given, answer);
    answer_key(answer, "stray");
    return 1;
}

static int answers_text_that_is_not_an_object(const weivel_json_value *given, answer_writer *answer) {
    (void)given;
    answer_string(answer, "supported");
    return 1;
}

static int calls_made = 0;

static int counts_its_calls(const weivel_json_value *given, answer_writer *answer) {
    calls_made++;
    return answers_supported(given, answer);
}

static verdict_status judged_as(const char *then_text, vector_tier tier, op_adapter adapter) {
    vector candidate = make_vector(then_text, tier);
    verdict judged;
    verdict_status status;
    judge_vector(&candidate, adapter, &judged);
    status = judged.status;
    free(judged.reasons);
    weivel_json_free(candidate.document);
    return status;
}

static void passes_when_the_answer_matches(void) {
    CHECK(judged_as("{\"supported\":true}", TIER_CORE, answers_supported) == VERDICT_PASSED);
}

static void fails_when_the_answer_differs(void) {
    CHECK(judged_as("{\"supported\":false}", TIER_CORE, answers_supported) == VERDICT_FAILED);
}

static void errors_a_vector_whose_adapter_answers_nothing(void) {
    CHECK(judged_as("{\"supported\":true}", TIER_CORE, answers_nothing) == VERDICT_ERRORED);
}

static void errors_a_vector_whose_op_has_no_adapter(void) {
    CHECK(judged_as("{\"supported\":true}", TIER_CORE, NULL) == VERDICT_ERRORED);
}

static void errors_an_incomplete_answer(void) {
    CHECK(judged_as("{\"supported\":true}", TIER_CORE, answers_half_a_document) == VERDICT_ERRORED);
}

static void errors_an_answer_the_writer_marks_broken_even_when_it_parses(void) {
    CHECK(judged_as("{\"supported\":true}", TIER_CORE, answers_a_valid_document_after_a_misuse) == VERDICT_ERRORED);
}

static void errors_an_answer_that_is_not_an_object(void) {
    CHECK(judged_as("{\"supported\":true}", TIER_CORE, answers_text_that_is_not_an_object) == VERDICT_ERRORED);
}

static void awaits_a_standard_vector_without_running_it(void) {
    calls_made = 0;
    CHECK(judged_as("{\"supported\":true}", TIER_STANDARD, counts_its_calls) == VERDICT_AWAITING_STANDARD);
    CHECK(calls_made == 0);
}

static void awaits_a_layer_vector_without_running_it(void) {
    calls_made = 0;
    CHECK(judged_as("{\"supported\":true}", TIER_LAYER, counts_its_calls) == VERDICT_AWAITING_LAYER);
    CHECK(calls_made == 0);
}

static int exit_code_for(const verdict_status *statuses, size_t count, text_builder *out) {
    vector candidate = make_vector("{\"supported\":true}", TIER_CORE);
    verdict *verdicts = calloc(count == 0 ? 1 : count, sizeof *verdicts);
    size_t at;
    int exit_code;
    for (at = 0; at < count; at++) {
        verdicts[at].vector = &candidate;
        verdicts[at].status = statuses[at];
        verdicts[at].reasons = text_duplicate("something is wrong\n");
    }
    exit_code = report_write(verdicts, count, out);
    verdicts_free(verdicts, count);
    weivel_json_free(candidate.document);
    return exit_code;
}

static void exits_zero_only_when_every_vector_passed(void) {
    static const verdict_status ALL_PASSED[] = {VERDICT_PASSED, VERDICT_PASSED};
    static const verdict_status ONE_FAILED[] = {VERDICT_PASSED, VERDICT_FAILED};
    static const verdict_status ONE_ERRORED[] = {VERDICT_PASSED, VERDICT_ERRORED};
    static const verdict_status ONE_AWAITING_STANDARD[] = {VERDICT_PASSED, VERDICT_AWAITING_STANDARD};
    static const verdict_status ONE_AWAITING_LAYER[] = {VERDICT_PASSED, VERDICT_AWAITING_LAYER};
    text_builder out;
    text_init(&out);
    CHECK(exit_code_for(ALL_PASSED, 2, &out) == 0);
    text_release(&out);
    CHECK(exit_code_for(ONE_FAILED, 2, &out) != 0);
    text_release(&out);
    CHECK(exit_code_for(ONE_ERRORED, 2, &out) != 0);
    text_release(&out);
    CHECK(exit_code_for(ONE_AWAITING_STANDARD, 2, &out) != 0);
    text_release(&out);
    CHECK(exit_code_for(ONE_AWAITING_LAYER, 2, &out) != 0);
    text_release(&out);
}

static void an_empty_run_is_not_a_pass(void) {
    text_builder out;
    text_init(&out);
    CHECK(exit_code_for(NULL, 0, &out) != 0);
    CHECK(strstr(text_view(&out), "no vectors ran") != NULL);
    text_release(&out);
}

static void counts_awaiting_vectors_apart_from_passes(void) {
    static const verdict_status MIXED[] = {VERDICT_PASSED, VERDICT_AWAITING_STANDARD, VERDICT_AWAITING_STANDARD,
                                           VERDICT_AWAITING_LAYER};
    text_builder out;
    char row[256];
    text_init(&out);
    exit_code_for(MIXED, 4, &out);
    CHECK(strstr(text_view(&out), "2 standard vectors are awaiting the standard") != NULL);
    CHECK(strstr(text_view(&out), "1 layer vectors are awaiting a layer suite") != NULL);
    snprintf(row, sizeof row, "%-12s%22lu%22lu%22lu%22lu%22lu", "supports", 1ul, 0ul, 0ul, 2ul, 1ul);
    CHECK(strstr(text_view(&out), row) != NULL);
    text_release(&out);
}

static void lists_the_reasons_of_a_vector_that_did_not_pass(void) {
    static const verdict_status FAILED[] = {VERDICT_FAILED};
    text_builder out;
    text_init(&out);
    exit_code_for(FAILED, 1, &out);
    CHECK(strstr(text_view(&out), "FAILED supports/example (covers 12.1)") != NULL);
    CHECK(strstr(text_view(&out), "    something is wrong") != NULL);
    text_release(&out);
}

int main(void) {
    passes_when_the_answer_matches();
    fails_when_the_answer_differs();
    errors_a_vector_whose_adapter_answers_nothing();
    errors_a_vector_whose_op_has_no_adapter();
    errors_an_incomplete_answer();
    errors_an_answer_the_writer_marks_broken_even_when_it_parses();
    errors_an_answer_that_is_not_an_object();
    awaits_a_standard_vector_without_running_it();
    awaits_a_layer_vector_without_running_it();
    exits_zero_only_when_every_vector_passed();
    an_empty_run_is_not_a_pass();
    counts_awaiting_vectors_apart_from_passes();
    lists_the_reasons_of_a_vector_that_did_not_pass();
    return check_summary("runner_test");
}
