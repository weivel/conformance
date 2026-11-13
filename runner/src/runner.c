#include "runner.h"

#include "match.h"

#include <stdlib.h>
#include <string.h>

const char *verdict_label(verdict_status status) {
    switch (status) {
    case VERDICT_PASSED:
        return "passed";
    case VERDICT_FAILED:
        return "failed";
    case VERDICT_ERRORED:
        return "errored";
    case VERDICT_AWAITING_STANDARD:
        return "awaiting the standard";
    case VERDICT_AWAITING_LAYER:
        return "awaiting a layer";
    }
    return "unknown";
}

static void conclude(verdict *judged, verdict_status status, const char *reasons) {
    judged->status = status;
    judged->reasons = text_duplicate(reasons);
}

static void judge_answer(const vector *candidate, const char *answer_text, verdict *judged) {
    weivel_json_error parse_error = {0, 0, NULL};
    weivel_json_value *answer = weivel_json_parse(answer_text, strlen(answer_text), &parse_error);
    text_builder reasons;
    if (answer == NULL) {
        text_init(&reasons);
        text_append_format(&reasons, "the answer is not readable JSON: %s\n",
                           parse_error.message == NULL ? "malformed" : parse_error.message);
        conclude(judged, VERDICT_ERRORED, text_view(&reasons));
        text_release(&reasons);
        return;
    }
    if (weivel_json_kind_of(answer) != WEIVEL_JSON_OBJECT) {
        conclude(judged, VERDICT_ERRORED, "the answer is not an object\n");
        weivel_json_free(answer);
        return;
    }
    text_init(&reasons);
    if (match_then(candidate->then, answer, &reasons) == 0) {
        conclude(judged, VERDICT_PASSED, "");
    } else {
        conclude(judged, VERDICT_FAILED, text_view(&reasons));
    }
    text_release(&reasons);
    weivel_json_free(answer);
}

static void judge_with_adapter(const vector *candidate, op_adapter adapter, verdict *judged) {
    answer_writer writer;
    answer_init(&writer);
    if (!adapter(candidate->given, &writer)) {
        conclude(judged, VERDICT_ERRORED, "the adapter answered nothing for this vector\n");
    } else if (!answer_is_complete(&writer)) {
        conclude(judged, VERDICT_ERRORED, "the adapter's answer is not a complete document\n");
    } else {
        judge_answer(candidate, answer_text(&writer), judged);
    }
    answer_release(&writer);
}

void judge_vector(const vector *candidate, op_adapter adapter, verdict *judged) {
    judged->vector = candidate;
    judged->reasons = NULL;
    if (candidate->tier == TIER_STANDARD) {
        conclude(judged, VERDICT_AWAITING_STANDARD, "");
    } else if (candidate->tier == TIER_LAYER) {
        conclude(judged, VERDICT_AWAITING_LAYER, "");
    } else if (adapter == NULL) {
        conclude(judged, VERDICT_ERRORED, "no adapter answers this op yet\n");
    } else {
        judge_with_adapter(candidate, adapter, judged);
    }
}

verdict *run_corpus(const corpus *loaded) {
    verdict *verdicts = calloc(loaded->count, sizeof *verdicts);
    size_t at;
    if (verdicts == NULL) {
        return NULL;
    }
    for (at = 0; at < loaded->count; at++) {
        const op_entry *entry = ops_find(loaded->items[at].op);
        judge_vector(&loaded->items[at], entry == NULL ? NULL : entry->adapter, &verdicts[at]);
    }
    return verdicts;
}

void verdicts_free(verdict *verdicts, size_t count) {
    size_t at;
    for (at = 0; at < count; at++) {
        free(verdicts[at].reasons);
    }
    free(verdicts);
}
