#include "fixture.h"

#include <stdlib.h>

static int compare_longs(const void *left, const void *right) {
    return *(const long *)left < *(const long *)right ? -1 : *(const long *)left > *(const long *)right;
}

long fixture_plant(void) {
    long values[2] = {2, 1};
    qsort(values, 2, sizeof values[0], compare_longs);
    return values[0];
}
