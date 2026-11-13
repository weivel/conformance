#include "check.h"
#include "match.h"

#include <stdlib.h>

static size_t failures_of(const char *then_text, const char *result_text, text_builder *reasons) {
    weivel_json_value *then = weivel_json_parse(then_text, strlen(then_text), NULL);
    weivel_json_value *result = weivel_json_parse(result_text, strlen(result_text), NULL);
    size_t failures = 0;
    CHECK(then != NULL);
    CHECK(result != NULL);
    if (then != NULL && result != NULL) {
        failures = match_then(then, result, reasons);
    }
    weivel_json_free(then);
    weivel_json_free(result);
    return failures;
}

static size_t failures_quietly(const char *then_text, const char *result_text) {
    text_builder reasons;
    size_t failures;
    text_init(&reasons);
    failures = failures_of(then_text, result_text, &reasons);
    text_release(&reasons);
    return failures;
}

static void holds(const char *then_text, const char *result_text, const char *label) {
    text_builder reasons;
    text_init(&reasons);
    if (failures_of(then_text, result_text, &reasons) != 0) {
        fprintf(stderr, "%s: expected to hold, but %s", label, text_view(&reasons));
        check_failures++;
    }
    text_release(&reasons);
}

static void fails(const char *then_text, const char *result_text, const char *label) {
    if (failures_quietly(then_text, result_text) == 0) {
        fprintf(stderr, "%s: expected not to hold\n", label);
        check_failures++;
    }
}

static void null_holds_for_an_absent_or_null_key(void) {
    holds("{\"refusal\":null}", "{}", "absent");
    holds("{\"refusal\":null}", "{\"refusal\":null}", "null");
    fails("{\"refusal\":null}", "{\"refusal\":{\"code\":\"undeclared\"}}", "present");
}

static void code_holds_when_the_refusal_carries_it(void) {
    holds("{\"refusal\":{\"code\":\"undeclared\"}}", "{\"refusal\":{\"code\":\"undeclared\",\"detail\":\"d\"}}", "same code");
    fails("{\"refusal\":{\"code\":\"undeclared\"}}", "{\"refusal\":{\"code\":\"wrong_provider\"}}", "other code");
    fails("{\"refusal\":{\"code\":\"undeclared\"}}", "{\"refusal\":null}", "no refusal");
    fails("{\"refusal\":{\"code\":\"undeclared\"}}", "{}", "refusal absent");
    fails("{\"refusal\":{\"code\":\"undeclared\"}}", "{\"refusal\":\"undeclared\"}", "refusal is text");
}

static void a_dropped_wording_assertion_is_not_an_assertion(void) {
    fails("{\"refusal\":{\"mentions\":[\"a\"]}}", "{\"refusal\":{\"detail\":\"a\"}}", "mentions");
}

static void subject_holds_when_the_refusal_names_it(void) {
    holds("{\"refusal\":{\"code\":\"undeclared\",\"subject\":\"a:b\"}}",
          "{\"refusal\":{\"code\":\"undeclared\",\"subject\":\"a:b\"}}", "same subject");
    fails("{\"refusal\":{\"subject\":\"a:b\"}}", "{\"refusal\":{\"subject\":\"a:c\"}}", "other subject");
    fails("{\"refusal\":{\"subject\":\"a:b\"}}", "{\"refusal\":{\"detail\":\"a:b\"}}", "subject in the wording");
    fails("{\"refusal\":{\"subject\":\"a:b\"}}", "{\"refusal\":null}", "no refusal");
    holds("{\"refusal\":{\"subject\":null}}", "{\"refusal\":{\"subject\":null}}", "no subject");
    fails("{\"refusal\":{\"subject\":null}}", "{\"refusal\":{\"code\":\"stale_handle\"}}", "subject not answered");
    fails("{\"refusal\":{\"subject\":null}}", "{\"refusal\":{\"subject\":\"a\"}}", "a subject where none is");
}

static void has_fix_holds_when_the_refusal_answers_it(void) {
    holds("{\"refusal\":{\"hasFix\":true}}", "{\"refusal\":{\"hasFix\":true}}", "has a fix");
    holds("{\"report\":{\"hasFix\":false}}", "{\"report\":{\"hasFix\":false}}", "has none");
    fails("{\"refusal\":{\"hasFix\":true}}", "{\"refusal\":{\"hasFix\":false}}", "answered none");
    fails("{\"refusal\":{\"hasFix\":true}}", "{\"refusal\":{\"fix\":\"f\"}}", "inferred from the wording");
    fails("{\"refusal\":{\"hasFix\":false}}", "{\"refusal\":{}}", "not answered");
}

static void an_unknown_assertion_beside_a_known_one_fails(void) {
    fails("{\"refusal\":{\"code\":\"undeclared\",\"mentions\":[\"a\"]}}", "{\"refusal\":{\"code\":\"undeclared\"}}",
          "mixed block");
}

static void before_holds_when_the_order_is_respected(void) {
    holds("{\"order\":{\"before\":[[\"a\",\"b\"]]}}", "{\"order\":[\"a\",\"c\",\"b\"]}", "in order");
    holds("{\"order\":{\"before\":[[\"a\",\"b\"],[\"c\",\"d\"]]}}", "{\"order\":[\"a\",\"c\",\"b\",\"d\"]}", "two pairs");
    fails("{\"order\":{\"before\":[[\"b\",\"a\"]]}}", "{\"order\":[\"a\",\"b\"]}", "reversed");
    fails("{\"order\":{\"before\":[[\"a\",\"z\"]]}}", "{\"order\":[\"a\",\"b\"]}", "member missing");
    fails("{\"order\":{\"before\":[[\"a\",\"b\"]]}}", "{\"order\":\"ab\"}", "not an array");
}

static void contains_holds_when_every_element_is_there(void) {
    holds("{\"paths\":{\"contains\":[\"api\",\"entry\"]}}", "{\"paths\":[\"entry\",\"x\",\"api\"]}", "both present");
    fails("{\"paths\":{\"contains\":[\"api\",\"entry\"]}}", "{\"paths\":[\"api\"]}", "one missing");
    fails("{\"paths\":{\"contains\":[\"api\"]}}", "{\"paths\":\"api\"}", "not an array");
    holds("{\"heard\":{\"contains\":[[\"a\",\"provide\"]]}}", "{\"heard\":[[\"b\",\"x\"],[\"a\",\"provide\"]]}", "structured element");
}

static void equals_holds_on_exact_equality(void) {
    holds("{\"started\":{\"equals\":[]}}", "{\"started\":[]}", "empty");
    fails("{\"started\":{\"equals\":[]}}", "{\"started\":[\"a\"]}", "not empty");
    holds("{\"started\":{\"equals\":[\"a\",\"b\"]}}", "{\"started\":[\"a\",\"b\"]}", "same order");
    fails("{\"started\":{\"equals\":[\"a\",\"b\"]}}", "{\"started\":[\"b\",\"a\"]}", "other order");
    fails("{\"started\":{\"equals\":[]}}", "{}", "absent key");
}

static void absent_holds_for_an_absent_or_null_key(void) {
    holds("{\"report\":{\"absent\":true}}", "{}", "absent");
    holds("{\"report\":{\"absent\":true}}", "{\"report\":null}", "null");
    fails("{\"report\":{\"absent\":true}}", "{\"report\":\"x\"}", "present");
}

static void present_holds_for_a_non_null_key(void) {
    holds("{\"report\":{\"present\":true}}", "{\"report\":\"x\"}", "present");
    holds("{\"report\":{\"present\":true}}", "{\"report\":[]}", "empty array");
    fails("{\"report\":{\"present\":true}}", "{}", "absent");
    fails("{\"report\":{\"present\":true}}", "{\"report\":null}", "null");
}

static void anything_else_is_deep_equality(void) {
    holds("{\"served\":true,\"atPoll\":2}", "{\"served\":true,\"atPoll\":2,\"extra\":1}", "scalars");
    fails("{\"served\":true}", "{\"served\":false}", "other boolean");
    fails("{\"atPoll\":2}", "{\"atPoll\":3}", "other number");
    fails("{\"atPoll\":2}", "{\"atPoll\":\"2\"}", "number against text");
    holds("{\"written\":{\"key\":\"k\",\"layer\":null}}", "{\"written\":{\"layer\":null,\"key\":\"k\"}}", "object by key");
    fails("{\"written\":{\"key\":\"k\"}}", "{\"written\":{\"key\":\"k\",\"layer\":null}}", "object with extra member");
    holds("{\"outcomes\":[\"served\",[\"a\"]]}", "{\"outcomes\":[\"served\",[\"a\"]]}", "nested");
    fails("{\"outcomes\":[\"served\",[\"a\"]]}", "{\"outcomes\":[\"served\",[\"b\"]]}", "nested difference");
}

static void a_refusal_axis_is_compared_by_value(void) {
    holds("{\"axis\":\"application\"}", "{\"axis\":\"application\"}", "same axis");
    fails("{\"axis\":\"application\"}", "{\"axis\":\"api\"}", "other axis");
}

static void a_key_absent_from_then_asserts_nothing(void) {
    holds("{\"served\":true}", "{\"served\":true,\"anything\":[1,2,3]}", "extra answer key");
}

static void every_failing_assertion_is_reported(void) {
    CHECK(failures_quietly("{\"a\":1,\"b\":2,\"c\":3}", "{\"a\":9,\"b\":2,\"c\":9}") == 2);
}

static void a_reason_names_the_key_and_what_was_answered(void) {
    text_builder reasons;
    text_init(&reasons);
    failures_of("{\"atPoll\":2}", "{\"atPoll\":3}", &reasons);
    CHECK(strstr(text_view(&reasons), "atPoll") != NULL);
    CHECK(strstr(text_view(&reasons), "3") != NULL);
    text_release(&reasons);
}

int main(void) {
    null_holds_for_an_absent_or_null_key();
    code_holds_when_the_refusal_carries_it();
    a_dropped_wording_assertion_is_not_an_assertion();
    subject_holds_when_the_refusal_names_it();
    has_fix_holds_when_the_refusal_answers_it();
    an_unknown_assertion_beside_a_known_one_fails();
    before_holds_when_the_order_is_respected();
    contains_holds_when_every_element_is_there();
    equals_holds_on_exact_equality();
    absent_holds_for_an_absent_or_null_key();
    present_holds_for_a_non_null_key();
    anything_else_is_deep_equality();
    a_refusal_axis_is_compared_by_value();
    a_key_absent_from_then_asserts_nothing();
    every_failing_assertion_is_reported();
    a_reason_names_the_key_and_what_was_answered();
    return check_summary("match_test");
}
