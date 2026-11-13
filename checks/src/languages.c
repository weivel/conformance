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
    if (strcmp(kind, "test-root") == 0) {
        return replace_text(&rules->test_root, value);
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

int language_rules_load(const char *path, const char *language, language_rules *rules, char *error,
                        size_t error_size) {
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
        if (language != NULL && strcmp(line_language, language) != 0) {
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

void language_rules_free(language_rules *rules) {
    text_list_free(&rules->extensions);
    text_list_free(&rules->names);
    text_list_free(&rules->patterns);
    text_list_free(&rules->prefixes);
    free(rules->source_root);
    free(rules->test_root);
    memset(rules, 0, sizeof *rules);
}
