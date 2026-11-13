#include "adapters.h"
#include "facts.h"
#include "session.h"

#include <stdint.h>

static int open_and_read_back(core_session *session, const weivel_manifest *facts, uint64_t payload,
                              uint64_t *handed_back) {
    weivel_handle plugin = 0;
    weivel_plugin_options options = WEIVEL_INIT(weivel_plugin_options);
    weivel_status status = weivel_register_manifest(session->core, facts, &plugin, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    options.payload = payload;
    status = weivel_open_plugin(session->core, plugin, &options, NULL);
    session_drain(session);
    if (status != WEIVEL_STATUS_OK) {
        return 0;
    }
    status = weivel_plugin_payload(session->core, plugin, handed_back, NULL);
    session_drain(session);
    return status == WEIVEL_STATUS_OK && !session->failed;
}

/**
 * @implNote The payload is the address of the vector's own manifest node, the host's manifest object, so an
 * equal copy handed back would answer false.
 */
int adapter_carry(const weivel_json_value *given, answer_writer *answer) {
    const weivel_json_value *manifest = weivel_json_object_get(given, "manifest");
    uint64_t payload = (uint64_t)(uintptr_t)manifest;
    uint64_t handed_back = 0;
    arena owner;
    weivel_manifest facts;
    core_session session;
    int answered = 0;
    arena_init(&owner);
    if (manifest != NULL && facts_manifest(&owner, manifest, &facts) && session_open(&session, NULL, NULL, NULL)) {
        answered = open_and_read_back(&session, &facts, payload, &handed_back);
        session_close(&session);
    }
    arena_release(&owner);
    if (answered) {
        answer_begin_object(answer);
        answer_key(answer, "sameObject");
        answer_bool(answer, handed_back == payload);
        answer_end_object(answer);
    }
    return answered;
}
