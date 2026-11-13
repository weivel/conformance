#include "standard_refusal.h"

static const char *const REFUSAL_NAMES[] = {
    "none",           "malformed_call",  "malformed_json",    "wrong_type", "unrepresentable",
    "missing_field",  "invalid_value",   "duplicate",         "not_well_known", "conflicting_field",
    "unreadable",     "not_found",       "name_mismatch",
};

const char *standard_refusal_name(uint32_t code) {
    return code < sizeof REFUSAL_NAMES / sizeof REFUSAL_NAMES[0] ? REFUSAL_NAMES[code] : NULL;
}

static void answer_text_or_null(answer_writer *answer, const char *text) {
    if (text == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, text);
    }
}

void standard_answer_refusal(answer_writer *answer, const weivel_standard_refusal *refusal) {
    const char *name = standard_refusal_name(refusal->code);
    if (refusal->code == WEIVEL_STANDARD_REFUSAL_NONE) {
        answer_null(answer);
        return;
    }
    answer_begin_object(answer);
    answer_key(answer, "code");
    if (name == NULL) {
        answer_int(answer, refusal->code);
    } else {
        answer_string(answer, name);
    }
    answer_key(answer, "detail");
    answer_text_or_null(answer, refusal->detail);
    answer_key(answer, "fix");
    answer_text_or_null(answer, refusal->fix);
    answer_key(answer, "path");
    answer_text_or_null(answer, refusal->path);
    answer_end_object(answer);
}
