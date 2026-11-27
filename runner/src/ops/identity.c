#include "adapters.h"

#include <weivel/identity.h>

#include <string.h>

/** The offset a refusal's detail names after `byte `, as `weivel_standard_identity_parse` documents; -1 for none. */
static int64_t offset_named_by(const char *detail) {
    static const char LEAD[] = "byte ";
    int64_t offset = 0;
    const char *at;
    if (detail == NULL || strncmp(detail, LEAD, sizeof LEAD - 1) != 0) {
        return -1;
    }
    at = detail + sizeof LEAD - 1;
    if (*at < '0' || *at > '9') {
        return -1;
    }
    while (*at >= '0' && *at <= '9') {
        offset = offset * 10 + (*at - '0');
        at++;
    }
    return *at == ':' ? offset : -1;
}

static int parse_value(const weivel_json_value *value, weivel_standard_identity *identity,
                       weivel_standard_refusal *refusal, weivel_status *status) {
    if (weivel_json_kind_of(value) != WEIVEL_JSON_STRING) {
        return 0;
    }
    *status = weivel_standard_identity_parse(weivel_json_as_string(value), weivel_json_string_length(value), identity,
                                             refusal);
    return *status == WEIVEL_STATUS_OK || *status == WEIVEL_STATUS_REFUSED;
}

static int answer_validity(const weivel_json_value *value, answer_writer *answer) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status = WEIVEL_STATUS_OK;
    int64_t offset = -1;
    int parsed = parse_value(value, &identity, &refusal, &status);
    if (parsed && status == WEIVEL_STATUS_REFUSED) {
        offset = offset_named_by(refusal.detail);
    }
    weivel_standard_identity_release(&identity);
    weivel_standard_refusal_release(&refusal);
    if (!parsed || (status == WEIVEL_STATUS_REFUSED && offset < 0)) {
        return 0;
    }
    answer_begin_object(answer);
    answer_key(answer, "valid");
    answer_bool(answer, status == WEIVEL_STATUS_OK);
    if (status == WEIVEL_STATUS_REFUSED) {
        answer_key(answer, "offset");
        answer_int(answer, offset);
    }
    answer_end_object(answer);
    return 1;
}

static int answer_order(const weivel_json_value *value, const weivel_json_value *other, answer_writer *answer) {
    weivel_standard_identity left = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_identity right = WEIVEL_INIT(weivel_standard_identity);
    weivel_status left_status = WEIVEL_STATUS_MALFORMED;
    weivel_status right_status = WEIVEL_STATUS_MALFORMED;
    int answered = parse_value(value, &left, NULL, &left_status) && parse_value(other, &right, NULL, &right_status) &&
                   left_status == WEIVEL_STATUS_OK && right_status == WEIVEL_STATUS_OK;
    if (answered) {
        int order = weivel_standard_identity_compare(&left, &right);
        answer_begin_object(answer);
        answer_key(answer, "order");
        answer_int(answer, order < 0 ? -1 : order > 0 ? 1 : 0);
        answer_end_object(answer);
    }
    weivel_standard_identity_release(&left);
    weivel_standard_identity_release(&right);
    return answered;
}

int adapter_identity(const weivel_json_value *given, answer_writer *answer) {
    const weivel_json_value *other = weivel_json_object_get(given, "other");
    if (other != NULL) {
        return answer_order(weivel_json_object_get(given, "value"), other, answer);
    }
    return answer_validity(weivel_json_object_get(given, "value"), answer);
}
