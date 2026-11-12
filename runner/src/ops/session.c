#include "session.h"

#include <stddef.h>

static const char *const REFUSAL_NAMES[] = {
    "none",          "malformed_call",    "stale_handle",   "malformed_manifest",  "incompatible",
    "name_taken",    "unmet_need",        "repeated_plugin", "plugin_state",       "identity_taken",
    "outside_namespace", "service_taken", "undeclared",     "wrong_provider",      "stopped",
    "timed_out",     "unmet_constraint",  "not_served",     "activation_mismatch", "provided_twice",
};

int session_open(core_session *session, const weivel_core_options *options, event_reader reader, void *context) {
    session->core = NULL;
    session->reader = reader;
    session->context = context;
    session->failed = 0;
    if (weivel_core_create(options, &session->core, NULL) != WEIVEL_STATUS_OK) {
        session->core = NULL;
        return 0;
    }
    session_drain(session);
    return 1;
}

void session_drain(core_session *session) {
    weivel_event event = WEIVEL_INIT(weivel_event);
    weivel_status status;
    while ((status = weivel_next_event(session->core, &event, NULL)) == WEIVEL_STATUS_OK) {
        if (session->reader != NULL) {
            session->reader(&event, session->context);
        }
        weivel_event_release(&event);
    }
    if (status != WEIVEL_STATUS_NONE) {
        session->failed = 1;
    }
}

void session_close(core_session *session) {
    weivel_core_destroy(session->core);
    session->core = NULL;
}

const char *session_refusal_name(uint32_t code) {
    return code < sizeof REFUSAL_NAMES / sizeof REFUSAL_NAMES[0] ? REFUSAL_NAMES[code] : NULL;
}

void session_answer_refusal(answer_writer *answer, uint32_t code, const char *detail, const char *fix) {
    const char *name = session_refusal_name(code);
    if (code == WEIVEL_REFUSAL_NONE) {
        answer_null(answer);
        return;
    }
    answer_begin_object(answer);
    answer_key(answer, "code");
    if (name == NULL) {
        answer_int(answer, code);
    } else {
        answer_string(answer, name);
    }
    answer_key(answer, "detail");
    if (detail == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, detail);
    }
    answer_key(answer, "fix");
    if (fix == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, fix);
    }
    answer_end_object(answer);
}
