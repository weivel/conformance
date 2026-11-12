#include "fixture.h"

FIXTURE_EXPORT long fixture_json_parse(const char *text);

long fixture_json_parse(const char *text) {
    return text[0] == '{';
}

long fixture_plant(void) {
    return fixture_json_parse("{}");
}
