#include "vectors.h"

#include "match.h"
#include "ops/ops.h"
#include "text.h"
#include "walk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *tier_name(vector_tier tier) {
    switch (tier) {
    case TIER_STANDARD:
        return "standard";
    case TIER_LAYER:
        return "layer";
    case TIER_CORE:
    default:
        return "core";
    }
}

static int tier_from_name(const char *name, size_t length, vector_tier *tier) {
    static const vector_tier TIERS[] = {TIER_CORE, TIER_STANDARD, TIER_LAYER};
    size_t at;
    for (at = 0; at < sizeof TIERS / sizeof TIERS[0]; at++) {
        const char *candidate = tier_name(TIERS[at]);
        if (strlen(candidate) == length && strncmp(candidate, name, length) == 0) {
            *tier = TIERS[at];
            return 1;
        }
    }
    return 0;
}

static int is_all_digits(const char *text) {
    if (*text == '\0') {
        return 0;
    }
    for (; *text != '\0'; text++) {
        if (*text < '0' || *text > '9') {
            return 0;
        }
    }
    return 1;
}

int covers_is_valid(const char *covers) {
    static const char INVARIANT[] = "invariant-";
    if (strncmp(covers, "12.", 3) == 0) {
        return covers[3] >= '1' && covers[3] <= '6' && covers[4] == '\0';
    }
    if (strncmp(covers, INVARIANT, sizeof INVARIANT - 1) == 0) {
        const char *digits = covers + sizeof INVARIANT - 1;
        int number;
        if (!is_all_digits(digits) || digits[0] == '0' || strlen(digits) > 2) {
            return 0;
        }
        number = atoi(digits);
        return number >= 1 && number <= 14;
    }
    return 0;
}

static int is_slug(const char *start, size_t length) {
    size_t at;
    if (length == 0 || start[0] == '-' || start[length - 1] == '-') {
        return 0;
    }
    for (at = 0; at < length; at++) {
        int lower = start[at] >= 'a' && start[at] <= 'z';
        int digit = start[at] >= '0' && start[at] <= '9';
        int hyphen = start[at] == '-' && start[at + 1] != '-';
        if (!lower && !digit && !hyphen) {
            return 0;
        }
    }
    return 1;
}

static int id_is_valid(const char *id) {
    const char *slash = strchr(id, '/');
    if (slash == NULL) {
        return 0;
    }
    return is_slug(id, (size_t)(slash - id)) && is_slug(slash + 1, strlen(slash + 1));
}

static int id_belongs_to_op(const char *id, const char *op) {
    size_t family = (size_t)(strchr(id, '/') - id);
    return strlen(op) == family && strncmp(id, op, family) == 0;
}

static char *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    long size;
    char *text;
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    text = malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    text[size] = '\0';
    *length = (size_t)size;
    return text;
}

static const json_value *object_member(const json_value *object, const char *key) {
    const json_value *member = json_object_get(object, key);
    return json_kind_of(member) == JSON_OBJECT ? member : NULL;
}

static int refuse(char *error, size_t error_size, const char *format, const char *path, const char *detail) {
    snprintf(error, error_size, format, path, detail);
    return 0;
}

static int check_shape(const vector *candidate, char *error, size_t error_size) {
    const char *path = candidate->path;
    if (candidate->op == NULL) {
        return refuse(error, error_size, "%s is not a valid vector: %s", path, "op must be a string");
    }
    if (ops_find(candidate->op) == NULL) {
        return refuse(error, error_size, "%s declares unknown op '%s', which no adapter can be asked to perform", path,
                      candidate->op);
    }
    if (candidate->id == NULL || !id_is_valid(candidate->id)) {
        return refuse(error, error_size, "%s is not a valid vector: %s", path, "id must read family/name in lowercase words");
    }
    if (candidate->covers == NULL || !covers_is_valid(candidate->covers)) {
        return refuse(error, error_size, "%s is not a valid vector: %s", path,
                      "covers must be 12.1 to 12.6 or invariant-1 to invariant-14");
    }
    if (candidate->given == NULL || candidate->then == NULL) {
        return refuse(error, error_size, "%s is not a valid vector: %s", path, "given and then must be objects");
    }
    if (match_refuse_meaningless(candidate->then) != NULL) {
        return refuse(error, error_size, "%s is not a valid vector: %s", path, match_refuse_meaningless(candidate->then));
    }
    if (json_object_size(candidate->then) == 0) {
        return refuse(error, error_size, "%s is not a valid vector: %s", path, "then asserts nothing");
    }
    if (!id_belongs_to_op(candidate->id, candidate->op)) {
        snprintf(error, error_size, "vector %s in %s runs op %s, so its id must start %s/", candidate->id, path,
                 candidate->op, candidate->op);
        return 0;
    }
    return 1;
}

static int is_separator(char character) {
    return character == '/' || character == '\\';
}

/**
 * Finds the last `core`, `standard` or `layer` directory in a file's full path. Returns the offset where that
 * directory's name starts, or -1 when the path carries none.
 */
static long tier_directory_offset(const char *path, vector_tier *tier) {
    long found = -1;
    size_t start = 0;
    size_t at;
    for (at = 0;; at++) {
        if (path[at] == '\0' || is_separator(path[at])) {
            vector_tier candidate;
            if (path[at] != '\0' && tier_from_name(path + start, at - start, &candidate)) {
                found = (long)start;
                *tier = candidate;
            }
            if (path[at] == '\0') {
                break;
            }
            start = at + 1;
        }
    }
    return found;
}

static int place(vector *candidate, const char *path, char *error, size_t error_size) {
    long offset = tier_directory_offset(path, &candidate->tier);
    const char *last_separator = path + strlen(path);
    text_builder name;
    size_t at;
    if (offset < 0) {
        return refuse(error, error_size, "%s is not in a core, standard or layer directory, so no suite owns it", path,
                      "");
    }
    while (last_separator > path && !is_separator(*last_separator)) {
        last_separator--;
    }
    text_init(&name);
    for (at = (size_t)offset; path + at < last_separator; at++) {
        text_append_bytes(&name, is_separator(path[at]) ? "/" : path + at, 1);
    }
    candidate->directory = text_duplicate(text_view(&name));
    text_release(&name);
    return candidate->directory != NULL;
}

static void vector_free(vector *candidate) {
    json_free(candidate->document);
    free(candidate->directory);
    free(candidate->path);
}

static int parse_document(vector *candidate, const char *path, char *error, size_t error_size) {
    size_t length = 0;
    char *text = read_file(path, &length);
    json_error parse_error = {0, 0, NULL};
    if (text == NULL) {
        return refuse(error, error_size, "%s is not readable: %s", path, "the file cannot be read");
    }
    candidate->document = json_parse(text, length, &parse_error);
    free(text);
    if (candidate->document == NULL) {
        snprintf(error, error_size, "%s is not readable JSON: %s (line %d, column %d)", path,
                 parse_error.message == NULL ? "malformed" : parse_error.message, parse_error.line, parse_error.column);
        return 0;
    }
    return 1;
}

static int read_vector(const char *path, vector *candidate, char *error, size_t error_size) {
    memset(candidate, 0, sizeof *candidate);
    if (!parse_document(candidate, path, error, error_size)) {
        return 0;
    }
    candidate->path = text_duplicate(path);
    candidate->id = json_as_string(json_object_get(candidate->document, "id"));
    candidate->op = json_as_string(json_object_get(candidate->document, "op"));
    candidate->covers = json_as_string(json_object_get(candidate->document, "covers"));
    candidate->given = object_member(candidate->document, "given");
    candidate->then = object_member(candidate->document, "then");
    return candidate->path != NULL && place(candidate, path, error, error_size);
}

static int compare_by_id(const void *left, const void *right) {
    const vector *first = left;
    const vector *second = right;
    int order = strcmp(first->id, second->id);
    return order != 0 ? order : strcmp(first->path, second->path);
}

static int refuse_duplicate_ids(const corpus *loaded, char *error, size_t error_size) {
    size_t at;
    for (at = 1; at < loaded->count; at++) {
        if (strcmp(loaded->items[at - 1].id, loaded->items[at].id) == 0) {
            snprintf(error, error_size, "duplicate vector id %s, in %s and in %s", loaded->items[at].id,
                     loaded->items[at - 1].path, loaded->items[at].path);
            return 0;
        }
    }
    return 1;
}

static int load_all(const path_list *paths, corpus *loaded, char *error, size_t error_size) {
    size_t at;
    loaded->items = calloc(paths->count, sizeof(vector));
    if (loaded->items == NULL) {
        snprintf(error, error_size, "out of memory");
        return 0;
    }
    for (at = 0; at < paths->count; at++) {
        vector *candidate = &loaded->items[loaded->count];
        int read = read_vector(paths->paths[at], candidate, error, error_size);
        loaded->count++;
        if (!read || !check_shape(candidate, error, error_size)) {
            return 0;
        }
    }
    return 1;
}

int corpus_load(const char *directory, corpus *loaded, char *error, size_t error_size) {
    path_list paths;
    int ok;
    loaded->items = NULL;
    loaded->count = 0;
    if (!walk_json_files(directory, &paths, error, error_size)) {
        return 0;
    }
    if (paths.count == 0) {
        snprintf(error, error_size, "no vectors under %s, which is not a pass", directory);
        path_list_free(&paths);
        return 0;
    }
    ok = load_all(&paths, loaded, error, error_size);
    path_list_free(&paths);
    if (ok) {
        qsort(loaded->items, loaded->count, sizeof(vector), compare_by_id);
        ok = refuse_duplicate_ids(loaded, error, error_size);
    }
    if (!ok) {
        corpus_free(loaded);
    }
    return ok;
}

void corpus_free(corpus *loaded) {
    size_t at;
    for (at = 0; at < loaded->count; at++) {
        vector_free(&loaded->items[at]);
    }
    free(loaded->items);
    loaded->items = NULL;
    loaded->count = 0;
}

size_t corpus_count_in(const corpus *loaded, const char *directory) {
    size_t at;
    size_t count = 0;
    for (at = 0; at < loaded->count; at++) {
        if (strcmp(loaded->items[at].directory, directory) == 0) {
            count++;
        }
    }
    return count;
}
