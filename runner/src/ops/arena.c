#include "arena.h"

#include <stdint.h>
#include <stdlib.h>

struct arena_block {
    arena_block *next;
};

void arena_init(arena *owner) {
    owner->blocks = NULL;
    owner->failed = 0;
}

void arena_release(arena *owner) {
    while (owner->blocks != NULL) {
        arena_block *next = owner->blocks->next;
        free(owner->blocks);
        owner->blocks = next;
    }
    owner->failed = 0;
}

#define ARENA_ALIGNMENT 16

static size_t aligned_header_size(void) {
    return (sizeof(arena_block) + ARENA_ALIGNMENT - 1) / ARENA_ALIGNMENT * ARENA_ALIGNMENT;
}

void *arena_take(arena *owner, size_t count, size_t size) {
    size_t header = aligned_header_size();
    arena_block *block;
    if (owner->failed || count == 0) {
        return NULL;
    }
    if (size != 0 && count > (SIZE_MAX - header) / size) {
        owner->failed = 1;
        return NULL;
    }
    block = calloc(1, header + count * size);
    if (block == NULL) {
        owner->failed = 1;
        return NULL;
    }
    block->next = owner->blocks;
    owner->blocks = block;
    return (char *)block + header;
}
