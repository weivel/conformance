#include "fixture.h"

#include <time.h>

long fixture_plant(void) {
    return (long)time(NULL);
}
