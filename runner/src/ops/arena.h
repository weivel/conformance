#ifndef CONFORMANCE_OPS_ARENA_H
#define CONFORMANCE_OPS_ARENA_H

#include <stddef.h>

typedef struct arena_block arena_block;

/**
 * Owns every array an adapter builds for one call into the core, so one release frees them all.
 * @implNote After an allocation failure the arena is `failed` and every later take answers NULL, so an adapter
 * checks once, before it calls the core, instead of after every take.
 */
typedef struct {
    arena_block *blocks;
    int failed;
} arena;

void arena_init(arena *owner);
void arena_release(arena *owner);

/** Zeroed storage for `count` elements of `size` bytes, NULL for a count of 0 or after a failure. */
void *arena_take(arena *owner, size_t count, size_t size);

#endif
