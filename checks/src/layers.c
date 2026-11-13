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
    if (!tree_list_every_file(directory, &files, error, error_size)) {
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

static int is_source_file(const char *relative, const language_rules *rules) {
    size_t at;
    for (at = 0; at < rules->extensions.count; at++) {
        if (ends_with(relative, rules->extensions.items[at])) {
            return 1;
        }
    }
    return 0;
}

/**
 * Marks every id a source file of the language under the test root names.
 *
 * @implNote Only the language's own source files count: a vector copied under the tests as a fixture carries its id
 * and would otherwise pass for a test that witnesses it. A directory directly under the test root that a repository
 * walk skips (`build`, `node_modules` and the rest) is skipped here too, so build output cannot witness a vector.
 */
static int mark_named_ids(const char *test_directory, const language_rules *rules, vector_ids *vectors, char *error,
                          size_t error_size) {
    text_list files;
    size_t file;
    if (!tree_list_files(test_directory, &files, error, error_size)) {
        return 0;
    }
    for (file = 0; file < files.count; file++) {
        char *path;
        char *data = NULL;
        size_t size = 0;
        size_t id;
        if (!is_source_file(files.items[file], rules)) {
            continue;
        }
        path = tree_join(test_directory, files.items[file]);
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

static int load_language(const char *languages_path, const char *language, language_rules *rules, char *error,
                         size_t error_size) {
    if (!language_rules_load(languages_path, language, rules, error, error_size)) {
        return 0;
    }
    if (!rules->known) {
        snprintf(error, error_size, "%s names no language '%s', so its tests have nowhere to be found", languages_path,
                 language);
    } else if (rules->test_root == NULL) {
        snprintf(error, error_size, "%s gives %s no test-root, so its tests have nowhere to be found", languages_path,
                 language);
    } else if (rules->extensions.count == 0) {
        snprintf(error, error_size, "%s gives %s no extension, so none of its tests can be read", languages_path,
                 language);
    } else {
        return 1;
    }
    language_rules_free(rules);
    return 0;
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

static int check(const char *root, const char *vector_directory, const language_rules *rules, char *error,
                 size_t error_size) {
    vector_ids vectors;
    char *test_directory;
    int offenders = -1;
    if (!load_vector_ids(vector_directory, &vectors, error, error_size)) {
        return -1;
    }
    test_directory = tree_join(root, rules->test_root);
    if (test_directory == NULL) {
        snprintf(error, error_size, "out of memory");
    } else if (mark_named_ids(test_directory, rules, &vectors, error, error_size)) {
        offenders = report_unnamed(&vectors, rules->test_root);
    }
    free(test_directory);
    vector_ids_free(&vectors);
    return offenders;
}

int main(int argument_count, char **arguments) {
    char error[512] = "";
    char *root;
    char *vector_directory;
    language_rules rules;
    int offenders = -1;
    if (argument_count != 5) {
        fprintf(stderr, "usage: weivel-layers <root> <layer-vector-directory> <language> <languages-file>\n");
        return 2;
    }
    root = tree_normalize_root(arguments[1]);
    vector_directory = tree_normalize_root(arguments[2]);
    if (load_language(arguments[4], arguments[3], &rules, error, sizeof error)) {
        if (root != NULL && vector_directory != NULL) {
            offenders = check(root, vector_directory, &rules, error, sizeof error);
        }
        language_rules_free(&rules);
    }
    if (offenders < 0) {
        fprintf(stderr, "weivel-layers: %s\n", error[0] != '\0' ? error : "out of memory");
    }
    free(root);
    free(vector_directory);
    return offenders < 0 ? 2 : offenders > 0 ? 1 : 0;
}
