#include "ops.h"

#include <string.h>

static const op_entry OPS[] = {
    {"supports", NULL}, {"boot", NULL},   {"order", NULL},   {"resolve", NULL}, {"revoke", NULL},
    {"scope", NULL},    {"want", NULL},   {"reach", NULL},   {"unbound", NULL}, {"carry", NULL},
    {"consume", NULL},  {"identity", NULL}, {"validate", NULL},
};

const op_entry *ops_all(size_t *count) {
    *count = sizeof OPS / sizeof OPS[0];
    return OPS;
}

const op_entry *ops_find(const char *op) {
    size_t count;
    const op_entry *entries = ops_all(&count);
    size_t at;
    for (at = 0; at < count; at++) {
        if (strcmp(entries[at].op, op) == 0) {
            return &entries[at];
        }
    }
    return NULL;
}
