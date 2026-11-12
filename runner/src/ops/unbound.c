#include "adapters.h"
#include "facts.h"
#include "session.h"

static void write_services(answer_writer *answer, const weivel_service_list *list) {
    uint32_t at;
    answer_begin_object(answer);
    answer_key(answer, "services");
    answer_begin_array(answer);
    for (at = 0; at < list->ids.count; at++) {
        answer_string(answer, list->ids.items[at]);
    }
    answer_end_array(answer);
    answer_end_object(answer);
}

static int ask_unbound(core_session *session, const weivel_manifest *manifest, answer_writer *answer) {
    weivel_handle plugin = 0;
    weivel_service_list list = WEIVEL_INIT(weivel_service_list);
    weivel_status status = weivel_register_manifest(session->core, manifest, &plugin, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    status = weivel_unbound_services(session->core, plugin, &list, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    write_services(answer, &list);
    weivel_service_list_release(&list);
    return !session->failed;
}

int adapter_unbound(const json_value *given, answer_writer *answer) {
    arena owner;
    weivel_manifest manifest;
    core_session session;
    int answered = 0;
    arena_init(&owner);
    if (facts_manifest(&owner, json_object_get(given, "manifest"), &manifest) &&
        session_open(&session, NULL, NULL, NULL)) {
        answered = ask_unbound(&session, &manifest, answer);
        session_close(&session);
    }
    arena_release(&owner);
    return answered;
}
