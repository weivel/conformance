#ifndef CONFORMANCE_STUB_HOST_TALLY_H
#define CONFORMANCE_STUB_HOST_TALLY_H

/**
 * @file
 * The tally contract: a definition the stub host implements nowhere and publishes, in
 * `library-declares-a-contract.json`, under the capability `stub-host.tally`, so a `host/contract/` vector checks a
 * plugin's implementation of it against its description. A plugin providing the capability offers every operation
 * but the releases; `next` takes the run a `run` answered, so a stream's reading is an operation of the
 * implementation itself.
 */

#include <stdint.h>

#include "stub_host.h"
#include "weivel/services.h"

/** A run of counts being read, the implementation's from the `run` that answered it until release_run. */
typedef struct stub_host_tally_run stub_host_tally_run;

/** The tally contract. First shape ends at `release_refusal`. */
typedef struct stub_host_tally {
    uint32_t size;
    void *context;
    /**
     * How many times `label` was counted.
     *
     * @param label Required.
     * @param total answers the count. Required.
     * @param refusal the consumer's empty refusal, or NULL for none, freed by `release_refusal`.
     */
    weivel_status (*count)(void *context, const char *label, uint64_t *total, weivel_standard_service_refusal *refusal);
    /**
     * Opens a run of counts starting at `from`.
     *
     * @param run the consumer's empty handle, freed by `release_run`. Required.
     * @param refusal the consumer's empty refusal, or NULL for none, freed by `release_refusal`.
     */
    weivel_status (*run)(void *context, uint64_t from, stub_host_tally_run **run,
                         weivel_standard_service_refusal *refusal);
    /**
     * The next count of a run.
     *
     * @param run read during the call only. Required.
     * @param value answers the count. Required.
     */
    weivel_status (*next)(void *context, stub_host_tally_run *run, uint64_t *value);
    void (*release_run)(void *context, stub_host_tally_run *run);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} stub_host_tally;

/** The tally contract's description, written by weivel-describe. */
STUB_HOST_API const weivel_standard_description *stub_host_tally_description(void);

#endif
