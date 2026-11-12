#ifndef CONFORMANCE_OPS_CAST_H
#define CONFORMANCE_OPS_CAST_H

#include "arena.h"
#include "hearing.h"
#include "json.h"
#include "session.h"

/** The plugins one vector names, each opened on the core, found by the name the vector calls it. */
typedef struct {
    const char *const *names;
    weivel_handle *handles;
    uint32_t count;
} cast;

/**
 * Registers and opens each manifest in the order given, as a host loads its plugins. `names` and `manifests` hold
 * `count` entries and `names` must outlive the cast. 0 when any registration or opening was not answered OK.
 */
int cast_open(arena *owner, core_session *session, const char *const *names, const weivel_manifest *manifests,
              uint32_t count, cast *opened);

/** The handle of the plugin the vector names `name`, 0 for a name it never gave. */
weivel_handle cast_handle(const cast *opened, const char *name);

/** The name of the plugin `plugin` is, NULL for a handle the cast never got. */
const char *cast_name(const cast *opened, weivel_handle plugin);

/**
 * Opens a core whose events `log` hears, with a plugin `provider` that provides `service_id` and a plugin
 * `consumer` that consumes it. The id is well known to that core, so a bare id is not refused for its namespace.
 */
int session_open_pair(core_session *session, hearing *log, arena *owner, const char *service_id,
                      weivel_handle *provider, weivel_handle *consumer);

/** A step's string member, or NULL when it has none. */
const char *step_text(const json_value *step, const char *key);

/** Writes `[[plugin, id]]`, one pair per reach refusal the core reported, in the order reported. 0 for a plugin
 * the cast does not know. */
int write_reported(answer_writer *answer, const hearing *log, const cast *opened);

/** Writes the run's first refusal, or null. */
void write_first_refusal(answer_writer *answer, const hearing *log);

void write_text_or_null(answer_writer *answer, const char *text);

#endif
