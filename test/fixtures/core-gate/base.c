#include "fixture.h"

#include <stdlib.h>

void *fixture_create(unsigned long size) {
    return malloc(size);
}

void fixture_destroy(void *fixture) {
    free(fixture);
}
