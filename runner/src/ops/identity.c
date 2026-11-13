#include "adapters.h"

#include <weivel/identity.h>

int adapter_identity(const weivel_json_value *given, answer_writer *answer) {
    const weivel_json_value *value = weivel_json_object_get(given, "value");
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_status status;
    if (weivel_json_kind_of(value) != WEIVEL_JSON_STRING) {
        return 0;
    }
    status = weivel_standard_identity_parse(weivel_json_as_string(value), weivel_json_string_length(value), &identity,
                                            NULL);
    weivel_standard_identity_release(&identity);
    if (status != WEIVEL_STATUS_OK && status != WEIVEL_STATUS_REFUSED) {
        return 0;
    }
    answer_begin_object(answer);
    answer_key(answer, "valid");
    answer_bool(answer, status == WEIVEL_STATUS_OK);
    answer_end_object(answer);
    return 1;
}
