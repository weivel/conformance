#ifndef CONFORMANCE_VECTORS_H
#define CONFORMANCE_VECTORS_H

#include "json.h"

#include <stddef.h>

/**
 * Which suite a vector belongs to, decided by the directory it lives in: `core`, `standard` or `layer`.
 * @implNote The tier is read from the first directory under the walked root when that is a tier name, else from
 * the name of the walked root itself, so `run vectors/core` and `run vectors` agree. A directory outside both is
 * core.
 */
typedef enum { TIER_CORE, TIER_STANDARD, TIER_LAYER } vector_tier;

typedef struct {
    const char *id;
    const char *op;
    const char *covers;
    const json_value *given;
    const json_value *then;
    vector_tier tier;
    /** The directory the file lives in, led by its tier, such as `core/boot`. */
    char *directory;
    char *path;
    json_value *document;
} vector;

typedef struct {
    vector *items;
    size_t count;
} corpus;

/**
 * Reads every vector under a directory, ordered by id so a report does not depend on the file system.
 * Refuses, with `error` filled, a corpus the runner cannot be trusted to report on: no vector at all, a file
 * that is not JSON, an op the kit does not know, a vector that is malformed, an id declared twice, or an id
 * whose family is not its op. Returns 1 on success.
 */
int corpus_load(const char *directory, corpus *loaded, char *error, size_t error_size);

void corpus_free(corpus *loaded);

/** How many vectors live in a directory, named as `vector.directory` is, such as `core/boot`. */
size_t corpus_count_in(const corpus *loaded, const char *directory);

/** 1 when `covers` names a section of the design (`12.1` to `12.6`) or an invariant (`invariant-1` to `invariant-14`). */
int covers_is_valid(const char *covers);

const char *tier_name(vector_tier tier);

#endif
