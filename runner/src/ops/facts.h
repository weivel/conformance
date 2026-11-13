#ifndef CONFORMANCE_OPS_FACTS_H
#define CONFORMANCE_OPS_FACTS_H

#include "arena.h"
#include "weivel/json.h"

#include <weivel/core.h>

/**
 * Builds the core's values from a vector's JSON, as a host builds them from whatever it reads.
 *
 * Every function answers 1 when the JSON had the shape it reads and 0 when it did not; an absent or null member
 * is an empty list or the field's default, never a failure. Strings point into the JSON and arrays into `owner`,
 * so a built value lives as long as both.
 */

int facts_strings(arena *owner, const json_value *array, weivel_strings *strings);

/** `object` maps each name to a number, in the object's order. */
int facts_numbers(arena *owner, const json_value *object, weivel_numbers *numbers);

/** A boolean as a weivel_choice: true YES, false NO, absent or null DEFAULT. 0 for anything else. */
int facts_choice(const json_value *flag, uint32_t *choice);

/** Each entry an id, or `{ id, optional?, degradable?, provider?, constraints? }`. */
int facts_service_wants(arena *owner, const json_value *array, weivel_service_wants *wants);

/** Each entry an id, or `{ id, attributes? }`. */
int facts_service_offers(arena *owner, const json_value *array, weivel_service_offers *offers);

/**
 * A manifest's facts: `importName` (or `id` when it has none), `api`, `entry`, `targets`, `identity`,
 * `permissions`, and `capabilities` and `services`, each `{ provides, consumes }`. Any other member is ignored.
 */
int facts_manifest(arena *owner, const json_value *object, weivel_manifest *manifest);

#endif
