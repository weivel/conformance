#include "fixture.h"

long json_parse_document(const char *text);

long fixture_plant(void) {
    return json_parse_document("{}");
}
