#include "run.h"

#include <stdlib.h>
#include <string.h>

#define REFUSAL_FIRST_SHAPE (offsetof(weivel_standard_service_refusal, storage) + sizeof(void *))

char *stub_copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

int stub_is_empty_answer(uint32_t size, size_t first_shape, const void *storage) {
    return size >= first_shape && storage == NULL;
}

void stub_clear_beyond(void *answer, uint32_t size, size_t provider_size) {
    if (size > provider_size) {
        memset((char *)answer + provider_size, 0, size - provider_size);
    }
}

int stub_is_usable_refusal(const weivel_standard_service_refusal *refusal) {
    return refusal == NULL || stub_is_empty_answer(refusal->size, REFUSAL_FIRST_SHAPE, refusal->storage);
}

weivel_status stub_refuse(weivel_standard_service_refusal *refusal, const char *detail, const char *fix) {
    size_t detail_length = strlen(detail) + 1;
    size_t fix_length = strlen(fix) + 1;
    char *storage;
    if (refusal == NULL) {
        return WEIVEL_STATUS_REFUSED;
    }
    storage = malloc(detail_length + fix_length);
    if (storage == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(storage, detail, detail_length);
    memcpy(storage + detail_length, fix, fix_length);
    stub_clear_beyond(refusal, refusal->size, sizeof *refusal);
    refusal->detail = storage;
    refusal->fix = storage + detail_length;
    refusal->storage = storage;
    return WEIVEL_STATUS_REFUSED;
}

void stub_release_refusal(void *context, weivel_standard_service_refusal *refusal) {
    (void)context;
    if (refusal == NULL || refusal->size < REFUSAL_FIRST_SHAPE || refusal->storage == NULL) {
        return;
    }
    free(refusal->storage);
    refusal->detail = NULL;
    refusal->fix = NULL;
    refusal->storage = NULL;
}
