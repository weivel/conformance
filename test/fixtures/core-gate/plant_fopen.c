#include "fixture.h"

#include <stdio.h>

long fixture_plant(void) {
    FILE *file = fopen("fixture-plant", "rb");
    return file != NULL;
}
