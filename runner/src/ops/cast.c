#include "cast.h"

#include <string.h>

static int open_one(core_session *session, const weivel_manifest *manifest, weivel_handle *plugin) {
    weivel_status status = weivel_register_manifest(session->core, manifest, plugin, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    status = weivel_open_plugin(session->core, *plugin, NULL, NULL);
    session_drain(session);
    return status == WEIVEL_STATUS_OK;
}

int cast_open(arena *owner, core_session *session, const char *const *names, const weivel_manifest *manifests,
              uint32_t count, cast *opened) {
    uint32_t at;
    opened->names = names;
    opened->count = count;
    opened->handles = arena_take(owner, count, sizeof *opened->handles);
    if (count > 0 && opened->handles == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (names[at] == NULL || !open_one(session, &manifests[at], &opened->handles[at])) {
            return 0;
        }
    }
    return 1;
}

weivel_handle cast_handle(const cast *opened, const char *name) {
    uint32_t at;
    for (at = 0; name != NULL && at < opened->count; at++) {
        if (strcmp(opened->names[at], name) == 0) {
            return opened->handles[at];
        }
    }
    return 0;
}

const char *cast_name(const cast *opened, weivel_handle plugin) {
    uint32_t at;
    for (at = 0; at < opened->count; at++) {
        if (opened->handles[at] == plugin) {
            return opened->names[at];
        }
    }
    return NULL;
}

int session_open_pair(core_session *session, hearing *log, arena *owner, const char *service_id,
                      weivel_handle *provider, weivel_handle *consumer) {
    static const char *const NAMES[] = {"provider", "consumer"};
    weivel_core_options options = WEIVEL_INIT(weivel_core_options);
    weivel_manifest manifests[2] = {WEIVEL_INIT(weivel_manifest), WEIVEL_INIT(weivel_manifest)};
    weivel_service_offer offer = WEIVEL_INIT(weivel_service_offer);
    weivel_service_want need = WEIVEL_INIT(weivel_service_want);
    cast opened;
    offer.id = service_id;
    need.id = service_id;
    manifests[0].import_name = NAMES[0];
    manifests[0].services.provides.items = &offer;
    manifests[0].services.provides.count = 1;
    manifests[1].import_name = NAMES[1];
    manifests[1].services.consumes.items = &need;
    manifests[1].services.consumes.count = 1;
    options.well_known_services.items = &service_id;
    options.well_known_services.count = 1;
    if (!session_open(session, &options, hearing_read, log) ||
        !cast_open(owner, session, NAMES, manifests, 2, &opened)) {
        return 0;
    }
    *provider = opened.handles[0];
    *consumer = opened.handles[1];
    return 1;
}

const char *step_text(const weivel_json_value *step, const char *key) {
    return weivel_json_as_string(weivel_json_object_get(step, key));
}

int write_reported(answer_writer *answer, const hearing *log, const cast *opened) {
    size_t at;
    answer_begin_array(answer);
    for (at = 0; at < log->count; at++) {
        const heard_event *heard = &log->events[at];
        const char *plugin = cast_name(opened, heard->plugin);
        if (heard->kind != WEIVEL_EVENT_REACH_REFUSED) {
            continue;
        }
        if (plugin == NULL || heard->id == NULL) {
            return 0;
        }
        answer_begin_array(answer);
        answer_string(answer, plugin);
        answer_string(answer, heard->id);
        answer_end_array(answer);
    }
    answer_end_array(answer);
    return 1;
}

void write_first_refusal(answer_writer *answer, const hearing *log) {
    session_refusal refusal;
    refusal.code = log->first_refusal;
    refusal.detail = log->first_detail;
    refusal.fix = log->first_fix;
    refusal.subject = log->first_subject;
    refusal.has_fix = log->first_has_fix;
    session_answer_refusal(answer, &refusal);
}

void write_text_or_null(answer_writer *answer, const char *text) {
    if (text == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, text);
    }
}
