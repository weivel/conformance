#include "adapters.h"
#include "facts.h"
#include "session.h"

static const char *axis_name(uint32_t axis) {
    switch (axis) {
    case WEIVEL_AXIS_APPLICATION:
        return "application";
    case WEIVEL_AXIS_API:
        return "api";
    default:
        return NULL;
    }
}

static int host_options(arena *owner, const json_value *host, weivel_core_options *options) {
    int64_t api = 0;
    const json_value *level = json_object_get(host, "api");
    if (level != NULL && (!json_as_int64(level, &api) || api < 1 || api > UINT32_MAX)) {
        return 0;
    }
    options->api = (uint32_t)api;
    return facts_strings(owner, json_object_get(host, "applications"), &options->applications);
}

static void write_support(answer_writer *answer, const weivel_support *support) {
    const char *axis = axis_name(support->axis);
    answer_begin_object(answer);
    answer_key(answer, "supported");
    answer_bool(answer, support->supported != 0);
    answer_key(answer, "refusal");
    session_answer_refusal(answer, support->refusal.code, support->refusal.detail, support->refusal.fix);
    answer_key(answer, "axis");
    if (axis == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, axis);
    }
    answer_end_object(answer);
}

static int ask_supports(core_session *session, const weivel_manifest *facts, answer_writer *answer) {
    weivel_support support = WEIVEL_INIT(weivel_support);
    weivel_status status = weivel_supports(session->core, facts, &support, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    write_support(answer, &support);
    weivel_support_release(&support);
    return !session->failed;
}

int adapter_supports(const json_value *given, answer_writer *answer) {
    arena owner;
    weivel_core_options options = WEIVEL_INIT(weivel_core_options);
    weivel_manifest facts;
    core_session session;
    int answered = 0;
    arena_init(&owner);
    if (host_options(&owner, json_object_get(given, "host"), &options) &&
        facts_manifest(&owner, json_object_get(given, "facts"), &facts) &&
        session_open(&session, &options, NULL, NULL)) {
        answered = ask_supports(&session, &facts, answer);
        session_close(&session);
    }
    arena_release(&owner);
    return answered;
}
