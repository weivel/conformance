#include "standard_refusal.h"

/** Keyed on the header's constants, never on position, so a code appended or reordered cannot shift a name. */
static const char *const REFUSAL_NAMES[] = {
    [WEIVEL_STANDARD_REFUSAL_NONE] = "none",
    [WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL] = "malformed_call",
    [WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON] = "malformed_json",
    [WEIVEL_STANDARD_REFUSAL_WRONG_TYPE] = "wrong_type",
    [WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE] = "unrepresentable",
    [WEIVEL_STANDARD_REFUSAL_MISSING_FIELD] = "missing_field",
    [WEIVEL_STANDARD_REFUSAL_INVALID_VALUE] = "invalid_value",
    [WEIVEL_STANDARD_REFUSAL_DUPLICATE] = "duplicate",
    [WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN] = "not_well_known",
    [WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD] = "conflicting_field",
    [WEIVEL_STANDARD_REFUSAL_UNREADABLE] = "unreadable",
    [WEIVEL_STANDARD_REFUSAL_NOT_FOUND] = "not_found",
    [WEIVEL_STANDARD_REFUSAL_NAME_MISMATCH] = "name_mismatch",
};

const char *standard_refusal_name(uint32_t code) {
    return code < sizeof REFUSAL_NAMES / sizeof REFUSAL_NAMES[0] ? REFUSAL_NAMES[code] : NULL;
}

uint32_t standard_refusal_named_codes(void) {
    return (uint32_t)(sizeof REFUSAL_NAMES / sizeof REFUSAL_NAMES[0]);
}

static void answer_text_or_null(answer_writer *answer, const char *text) {
    if (text == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, text);
    }
}

static int is_said(const char *text) {
    return text != NULL && text[0] != '\0';
}

int standard_refusal_is_answerable(const weivel_standard_refusal *refusal) {
    return refusal->code == WEIVEL_STANDARD_REFUSAL_NONE || (is_said(refusal->detail) && is_said(refusal->fix));
}

int standard_answer_refusal(answer_writer *answer, const weivel_standard_refusal *refusal) {
    const char *name = standard_refusal_name(refusal->code);
    if (!standard_refusal_is_answerable(refusal)) {
        return 0;
    }
    if (refusal->code == WEIVEL_STANDARD_REFUSAL_NONE) {
        answer_null(answer);
        return 1;
    }
    answer_begin_object(answer);
    answer_key(answer, "code");
    if (name == NULL) {
        answer_int(answer, refusal->code);
    } else {
        answer_string(answer, name);
    }
    answer_key(answer, "detail");
    answer_string(answer, refusal->detail);
    answer_key(answer, "fix");
    answer_string(answer, refusal->fix);
    answer_key(answer, "path");
    answer_text_or_null(answer, refusal->path);
    answer_end_object(answer);
    return 1;
}
