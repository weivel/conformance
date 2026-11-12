#include "fixture.h"

#include <stdlib.h>

long fixture_plant(void) {
    return getenv("FIXTURE_PLANT") != NULL;
}
