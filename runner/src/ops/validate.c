#include "adapters.h"
#include "facts.h"
#include "json_text.h"
#include "standard_refusal.h"

#include <weivel/standard.h>

#include <string.h>

static const char PUBLISHED_SCHEMA[] = "plugin.schema.json";

/**
 * @implNote A vector naming any other schema is not answered: the standard validates against its one published
 * schema, so answering would judge the vector against a vocabulary it did not name.
 */
static int names_the_published_schema(const weivel_json_value *given) {
    const char *schema = weivel_json_as_string(weivel_json_object_get(given, "schema"));
    return schema != NULL && strcmp(schema, PUBLISHED_SCHEMA) == 0;
}

/** An absent list leaves `options` without one, so rule V2 is not applied; an empty list refuses every bare id. */
static int well_known_services(arena *owner, const weivel_json_value *given, weivel_strings *list,
                               weivel_standard_validation_options *options) {
    const weivel_json_value *member = weivel_json_object_get(given, "wellKnownServices");
    if (member == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(member) != WEIVEL_JSON_ARRAY || !facts_strings(owner, member, list)) {
        return 0;
    }
    options->well_known_services = list;
    return 1;
}

static void write_paths(answer_writer *answer, const weivel_strings *paths) {
    uint32_t at;
    answer_begin_array(answer);
    for (at = 0; at < paths->count; at++) {
        answer_string(answer, paths->items[at]);
    }
    answer_end_array(answer);
}

static void write_validation(answer_writer *answer, const weivel_standard_validation *validation) {
    answer_begin_object(answer);
    answer_key(answer, "valid");
    answer_bool(answer, validation->valid != 0);
    answer_key(answer, "paths");
    write_paths(answer, &validation->paths);
    answer_key(answer, "refusal");
    standard_answer_refusal(answer, &validation->refusal);
    answer_end_object(answer);
}

static int ask_validate(const char *manifest, const weivel_standard_validation_options *options,
                        answer_writer *answer) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_status status = weivel_standard_validate(manifest, strlen(manifest), options, &validation);
    int answered = status == WEIVEL_STATUS_OK || status == WEIVEL_STATUS_REFUSED;
    if (answered) {
        write_validation(answer, &validation);
    }
    weivel_standard_validation_release(&validation);
    return answered;
}

int adapter_validate(const weivel_json_value *given, answer_writer *answer) {
    arena owner;
    answer_writer manifest;
    weivel_strings list = {NULL, 0};
    weivel_standard_validation_options options = WEIVEL_INIT(weivel_standard_validation_options);
    const weivel_json_value *document = weivel_json_object_get(given, "manifest");
    int answered = 0;
    arena_init(&owner);
    answer_init(&manifest);
    if (document != NULL && names_the_published_schema(given) && well_known_services(&owner, given, &list, &options) &&
        json_text_write(document, &manifest) && answer_is_complete(&manifest)) {
        answered = ask_validate(answer_text(&manifest), &options, answer);
    }
    answer_release(&manifest);
    arena_release(&owner);
    return answered;
}
