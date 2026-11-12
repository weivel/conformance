#include "ops.h"

#include "adapters.h"

#include <string.h>

static const op_entry OPS[] = {
    {"supports", adapter_supports},
    {"boot", adapter_boot},
    {"order", adapter_order},
    {"resolve", adapter_resolve},
    {"revoke", adapter_revoke},
    {"scope", adapter_scope},
    {"want", adapter_want},
    {"reach", adapter_reach},
    {"unbound", adapter_unbound},
    {"carry", adapter_carry},
    {"consume", adapter_consume},
    {"identity", NULL},
    {"validate", NULL},
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
