#ifndef CONFORMANCE_STUB_HOST_H
#define CONFORMANCE_STUB_HOST_H

/**
 * @file
 * The kit's stub provider of `weivel.host` and `weivel.scope` (`weivel/host.h`, `weivel/scope.h` of the standard),
 * which a language's host program is run against by the `vectors/layer/host/` vectors. Its run is read from the
 * `stub-host.json` in the directory its `start` is handed as the first argument, and everything it is told is
 * appended to `record.txt` in that directory, as `docs/vectors/layer/host.md` defines both. It is a test library only.
 */

#include "weivel/description.h"

#if defined(_WIN32)
#if defined(STUB_HOST_BUILDING)
#define STUB_HOST_API __declspec(dllexport)
#else
#define STUB_HOST_API __declspec(dllimport)
#endif
#else
#define STUB_HOST_API __attribute__((visibility("default")))
#endif

/** The weivel_standard_host the library serves under `weivel.host`. */
STUB_HOST_API const void *stub_host_definition(void);

/** The weivel_standard_scopes the library serves under `weivel.scope`. */
STUB_HOST_API const void *stub_host_scopes_definition(void);

/**
 * The description of weivel_standard_host, written by weivel-describe from the standard's header and naming
 * stub_host_definition, since a description names the one function answering the definition it describes.
 */
STUB_HOST_API const weivel_standard_description *stub_host_description(void);

/**
 * The description of weivel_standard_scopes, written by weivel-describe, naming stub_host_scopes_definition and
 * nesting the standard's config, secrets, paths and log descriptions.
 */
STUB_HOST_API const weivel_standard_description *stub_host_scopes_description(void);

/**
 * A weivel_standard_log that keeps no entry, which `library-declares-log.json` declares under `weivel.log`: a plan
 * library declaring a standard id beside the scope, as `weivel/host.h` says makes a scope answering it unofferable.
 */
STUB_HOST_API const void *stub_host_log_definition(void);

/** The description of weivel_standard_log, written by weivel-describe, naming stub_host_log_definition. */
STUB_HOST_API const weivel_standard_description *stub_host_log_description(void);

#endif
