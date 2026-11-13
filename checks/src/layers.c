#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "languages.h"
#include "tree.h"

#define VECTOR_SUFFIX ".json"

typedef struct {
    text_list ids;
    int *named;
} vector_ids;

static int ends_with(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length > suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

/** A vector's id is its path under the layer directory without `.json`, the form the runner checks it against. */
static int load_vector_ids(const char *directory, vector_ids *vectors, char *error, size_t error_size) {
    text_list files;
    size_t at;
    memset(vectors, 0, sizeof *vectors);
    if (!tree_list_files(directory, &files, error, error_size)) {
        return 0;
    }
    for (at = 0; at < files.count; at++) {
        if (ends_with(files.items[at], VECTOR_SUFFIX)) {
            files.items[at][strlen(files.items[at]) - strlen(VECTOR_SUFFIX)] = '\0';
            if (!text_list_add(&vectors->ids, files.items[at])) {
                snprintf(error, error_size, "out of memory");
                text_list_free(&files);
                return 0;
            }
        }
    }
    text_list_free(&files);
    if (vectors->ids.count == 0) {
        snprintf(error, error_size, "no layer vectors under %s, so nothing was checked", directory);
        return 0;
    }
    vectors->named = calloc(vectors->ids.count, sizeof *vectors->named);
    if (vectors->named == NULL) {
        snprintf(error, error_size, "out of memory");
        text_list_free(&vectors->ids);
        return 0;
    }
    return 1;
}

static void vector_ids_free(vector_ids *vectors) {
    text_list_free(&vectors->ids);
    free(vectors->named);
}

static int is_id_character(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '/' ||
           character == '.';
}

/**
 * Whether `id` occurs in `text` standing on its own, so a longer id or a file path that merely contains it does not
 * count as naming it.
 */
static int names_id(const char *text, size_t size, const char *id) {
    size_t length = strlen(id);
    size_t at;
    for (at = 0; at + length <= size; at++) {
        int starts_alone = at == 0 || !is_id_character(text[at - 1]);
        int ends_alone = at + length == size || !is_id_character(text[at + length]);
        if (starts_alone && ends_alone && memcmp(text + at, id, length) == 0) {
            return 1;
        }
    }
    return 0;
}

static int mark_named_ids(const char *test_directory, vector_ids *vectors, char *error, size_t error_size) {
    text_list files;
    size_t file;
    if (!tree_list_files(test_directory, &files, error, error_size)) {
        return 0;
    }
    for (file = 0; file < files.count; file++) {
        char *path = tree_join(test_directory, files.items[file]);
        char *data = NULL;
        size_t size = 0;
        size_t id;
        if (path == NULL || !tree_read_file(path, &data, &size)) {
            snprintf(error, error_size, "%s cannot be read", files.items[file]);
            free(path);
            text_list_free(&files);
            return 0;
        }
        for (id = 0; id < vectors->ids.count; id++) {
            vectors->named[id] = vectors->named[id] || names_id(data, size, vectors->ids.items[id]);
        }
        free(data);
        free(path);
    }
    text_list_free(&files);
    return 1;
}

static char *resolve_test_root(const char *languages_path, const char *language, char *error, size_t error_size) {
    language_rules rules;
    char *test_root = NULL;
    if (!language_rules_load(languages_path, language, &rules, error, error_size)) {
        return NULL;
    }
    if (!rules.known) {
        snprintf(error, error_size, "%s names no language '%s', so its tests have nowhere to be found", languages_path,
                 language);
    } else if (rules.test_root == NULL) {
        snprintf(error, error_size, "%s gives %s no test-root, so its tests have nowhere to be found", languages_path,
                 language);
    } else {
        test_root = malloc(strlen(rules.test_root) + 1);
        if (test_root != NULL) {
            memcpy(test_root, rules.test_root, strlen(rules.test_root) + 1);
        }
    }
    language_rules_free(&rules);
    return test_root;
}

static int report_unnamed(const vector_ids *vectors, const char *test_root) {
    size_t at;
    int offenders = 0;
    for (at = 0; at < vectors->ids.count; at++) {
        if (!vectors->named[at]) {
            fprintf(stderr, "%s: no test under %s names this layer vector\n", vectors->ids.items[at], test_root);
            offenders++;
        }
    }
    if (offenders == 0) {
        printf("weivel-layers: every one of %zu layer vectors is named under %s\n", vectors->ids.count, test_root);
    }
    return offenders;
}

static int check(const char *root, const char *vector_directory, const char *test_root, char *error,
                 size_t error_size) {
    vector_ids vectors;
    char *test_directory;
    int offenders = -1;
    if (!load_vector_ids(vector_directory, &vectors, error, error_size)) {
        return -1;
    }
    test_directory = tree_join(root, test_root);
    if (test_directory == NULL) {
        snprintf(error, error_size, "out of memory");
    } else if (mark_named_ids(test_directory, &vectors, error, error_size)) {
        offenders = report_unnamed(&vectors, test_root);
    }
    free(test_directory);
    vector_ids_free(&vectors);
    return offenders;
}

int main(int argument_count, char **arguments) {
    char error[512] = "";
    char *root;
    char *vector_directory;
    char *test_root;
    int offenders = -1;
    if (argument_count != 5) {
        fprintf(stderr, "usage: weivel-layers <root> <layer-vector-directory> <language> <languages-file>\n");
        return 2;
    }
    root = tree_normalize_root(arguments[1]);
    vector_directory = tree_normalize_root(arguments[2]);
    test_root = resolve_test_root(arguments[4], arguments[3], error, sizeof error);
    if (root != NULL && vector_directory != NULL && test_root != NULL) {
        offenders = check(root, vector_directory, test_root, error, sizeof error);
    }
    if (offenders < 0) {
        fprintf(stderr, "weivel-layers: %s\n", error[0] != '\0' ? error : "out of memory");
    }
    free(root);
    free(vector_directory);
    free(test_root);
    return offenders < 0 ? 2 : offenders > 0 ? 1 : 0;
}
