#include "adapters.h"
#include "json_text.h"
#include "standard_refusal.h"

#include <weivel/library.h>

#include <string.h>

/** Keyed on the header's constants, never on position, so a kind appended cannot shift a name. */
static const char *const KIND_NAMES[] = {
    [WEIVEL_STANDARD_FUNCTION_BYTES] = "bytes",
    [WEIVEL_STANDARD_FUNCTION_DEFINITION] = "definition",
};

const char *library_kind_name(uint32_t kind) {
    return kind < sizeof KIND_NAMES / sizeof KIND_NAMES[0] ? KIND_NAMES[kind] : NULL;
}

static void answer_text_or_null(answer_writer *answer, const char *text) {
    if (text == NULL) {
        answer_null(answer);
    } else {
        answer_string(answer, text);
    }
}

static void write_function(answer_writer *answer, const weivel_standard_library_function *function) {
    const char *kind = library_kind_name(function->kind);
    answer_begin_object(answer);
    answer_key(answer, "name");
    answer_string(answer, function->name);
    answer_key(answer, "symbol");
    answer_string(answer, function->symbol);
    answer_key(answer, "kind");
    if (kind == NULL) {
        answer_int(answer, function->kind);
    } else {
        answer_string(answer, kind);
    }
    answer_end_object(answer);
}

static void write_services(answer_writer *answer, const weivel_standard_library *library) {
    size_t service;
    size_t function;
    answer_begin_array(answer);
    for (service = 0; service < library->service_count; service++) {
        answer_begin_object(answer);
        answer_key(answer, "id");
        answer_string(answer, library->services[service].id);
        answer_key(answer, "functions");
        answer_begin_array(answer);
        for (function = 0; function < library->services[service].function_count; function++) {
            write_function(answer, &library->services[service].functions[function]);
        }
        answer_end_array(answer);
        answer_end_object(answer);
    }
    answer_end_array(answer);
}

static int write_reading(answer_writer *answer, weivel_status status, const weivel_standard_library *library,
                         const weivel_standard_refusal *refusal) {
    int refusal_answered;
    answer_begin_object(answer);
    answer_key(answer, "read");
    answer_bool(answer, status == WEIVEL_STATUS_OK);
    if (status == WEIVEL_STATUS_OK) {
        answer_key(answer, "release");
        answer_text_or_null(answer, library->release);
        answer_key(answer, "services");
        write_services(answer, library);
    }
    answer_key(answer, "path");
    answer_text_or_null(answer, refusal->path);
    answer_key(answer, "refusal");
    refusal_answered = standard_answer_refusal(answer, refusal);
    answer_end_object(answer);
    return refusal_answered;
}

static int ask_read_library(const char *declaration, answer_writer *answer) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status = weivel_standard_read_library(declaration, strlen(declaration), &library, &refusal);
    int answered = (status == WEIVEL_STATUS_OK || status == WEIVEL_STATUS_REFUSED) &&
                   standard_refusal_is_answerable(&refusal) && write_reading(answer, status, &library, &refusal);
    weivel_standard_library_release(&library);
    weivel_standard_refusal_release(&refusal);
    return answered;
}

int adapter_library(const weivel_json_value *given, answer_writer *answer) {
    answer_writer declaration;
    const weivel_json_value *document = weivel_json_object_get(given, "declaration");
    int answered = 0;
    answer_init(&declaration);
    if (document != NULL && json_text_write(document, &declaration) && answer_is_complete(&declaration)) {
        answered = ask_read_library(answer_text(&declaration), answer);
    }
    answer_release(&declaration);
    return answered;
}
