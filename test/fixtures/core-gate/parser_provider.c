#include "fixture.h"

FIXTURE_EXPORT long json_parse_document(const char *text);

long json_parse_document(const char *text) {
    return text != 0;
}
