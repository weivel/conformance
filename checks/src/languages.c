#include "languages.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static text_list *list_for_kind(language_rules *rules, const char *kind) {
    if (strcmp(kind, "extension") == 0) {
        return &rules->extensions;
    }
    if (strcmp(kind, "name") == 0) {
        return &rules->names;
    }
    if (strcmp(kind, "pattern") == 0) {
        return &rules->patterns;
    }
    if (strcmp(kind, "prefix") == 0) {
        return &rules->prefixes;
    }
    return NULL;
}

static int replace_text(char **slot, const char *value) {
    char *copy = malloc(strlen(value) + 1);
    if (copy == NULL) {
        return 0;
    }
    memcpy(copy, value, strlen(value) + 1);
    free(*slot);
    *slot = copy;
    return 1;
}

static int apply_line(language_rules *rules, const char *kind, const char *value) {
    text_list *list;
    if (strcmp(kind, "source-root") == 0) {
        return replace_text(&rules->source_root, value);
    }
    if (strcmp(kind, "source-base") == 0) {
        return replace_text(&rules->source_base, value);
    }
    if (strcmp(kind, "test-root") == 0) {
        return replace_text(&rules->test_root, value);
    }
    if (strcmp(kind, "header-root") == 0) {
        return replace_text(&rules->header_root, value);
    }
    list = list_for_kind(rules, kind);
    return list != NULL && text_list_add(list, value);
}

static int split_line(char *line, char **language, char **kind, char **value) {
    char *first = strchr(line, ' ');
    char *second;
    if (first == NULL) {
        return 0;
    }
    *first = '\0';
    second = strchr(first + 1, ' ');
    if (second == NULL) {
        return 0;
    }
    *second = '\0';
    *language = line;
    *kind = first + 1;
    *value = second + 1;
    return (*value)[0] != '\0';
}

typedef enum { LINES_OF_THE_LANGUAGE, LINES_OF_THE_OTHERS } line_choice;

static int is_chosen(const char *line_language, const char *language, line_choice choice) {
    int same = language != NULL && strcmp(line_language, language) == 0;
    return choice == LINES_OF_THE_LANGUAGE ? language == NULL || same : !same;
}

static int load_chosen_lines(const char *path, const char *language, line_choice choice, language_rules *rules,
                             char *error, size_t error_size) {
    text_list lines;
    size_t at;
    memset(rules, 0, sizeof *rules);
    if (!tree_read_lines(path, &lines, error, error_size)) {
        return 0;
    }
    for (at = 0; at < lines.count; at++) {
        char *line_language;
        char *kind;
        char *value;
        char *line = lines.items[at];
        char original[256];
        snprintf(original, sizeof original, "%s", line);
        if (!split_line(line, &line_language, &kind, &value)) {
            snprintf(error, error_size, "%s: malformed line '%s'", path, original);
            text_list_free(&lines);
            language_rules_free(rules);
            return 0;
        }
        if (!is_chosen(line_language, language, choice)) {
            continue;
        }
        if (!apply_line(rules, kind, value)) {
            snprintf(error, error_size, "%s: unusable line '%s'", path, original);
            text_list_free(&lines);
            language_rules_free(rules);
            return 0;
        }
        rules->known = 1;
    }
    text_list_free(&lines);
    return 1;
}

int language_rules_load(const char *path, const char *language, language_rules *rules, char *error,
                        size_t error_size) {
    return load_chosen_lines(path, language, LINES_OF_THE_LANGUAGE, rules, error, error_size);
}

int language_rules_load_others(const char *path, const char *language, language_rules *rules, char *error,
                               size_t error_size) {
    return load_chosen_lines(path, language, LINES_OF_THE_OTHERS, rules, error, error_size);
}

void language_rules_free(language_rules *rules) {
    text_list_free(&rules->extensions);
    text_list_free(&rules->names);
    text_list_free(&rules->patterns);
    text_list_free(&rules->prefixes);
    free(rules->source_root);
    free(rules->source_base);
    free(rules->test_root);
    free(rules->header_root);
    memset(rules, 0, sizeof *rules);
}
