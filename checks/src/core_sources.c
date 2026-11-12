#include "core_sources.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

static int is_identifier_character(char character) {
    return isalnum((unsigned char)character) || character == '_';
}

static int is_scanned(const char *relative) {
    return strncmp(relative, "src/", 4) == 0 || strncmp(relative, "include/", 8) == 0;
}

static int equals_ignoring_case(const char *text, const char *name, size_t length) {
    size_t at;
    for (at = 0; at < length; at++) {
        if (text[at] == '\0' || tolower((unsigned char)text[at]) != tolower((unsigned char)name[at])) {
            return 0;
        }
    }
    return 1;
}

static int is_followed_by_a_call(const char *after) {
    while (*after == ' ' || *after == '\t' || *after == '\r' || *after == '\n') {
        after++;
    }
    return *after == '(';
}

static int matches_at(const rule *source_rule, const char *data, size_t at) {
    const char *name = source_rule->pattern;
    size_t length = strlen(name);
    if (source_rule->form == FORM_TEXT) {
        return equals_ignoring_case(data + at, name, length);
    }
    if (strncmp(data + at, name, length) != 0 || (at > 0 && is_identifier_character(data[at - 1]))) {
        return 0;
    }
    if (source_rule->form == FORM_WORD) {
        return !is_identifier_character(data[at + length]);
    }
    if (source_rule->form == FORM_CALL) {
        return is_followed_by_a_call(data + at + length);
    }
    return 1;
}

static size_t line_of(const char *data, size_t position) {
    size_t line = 1;
    size_t at;
    for (at = 0; at < position; at++) {
        line += data[at] == '\n';
    }
    return line;
}

static int applies_to(const rule_set *rules, const rule *source_rule, const char *relative) {
    if (source_rule->kind == RULE_FORBID_SOURCE) {
        return 1;
    }
    return source_rule->kind == RULE_SOURCE_IN_MUTEX_ONLY && strcmp(relative, rules->mutex_source) != 0;
}

static void report_hit(const rule_set *rules, const rule *source_rule, const char *relative, size_t line) {
    if (source_rule->kind == RULE_FORBID_SOURCE) {
        fprintf(stderr, "%s:%zu: '%s' reads the world (%s)\n", relative, line, source_rule->pattern,
            source_rule->reason);
    } else {
        fprintf(stderr, "%s:%zu: '%s' is a lock call outside %s (%s)\n", relative, line, source_rule->pattern,
            rules->mutex_source, source_rule->reason);
    }
}

static size_t scan_source(const rule_set *rules, const char *relative, const char *data) {
    size_t offenders = 0;
    size_t rule_at;
    size_t at;
    for (rule_at = 0; rule_at < rules->count; rule_at++) {
        const rule *source_rule = &rules->items[rule_at];
        if (!applies_to(rules, source_rule, relative)) {
            continue;
        }
        for (at = 0; data[at] != '\0'; at++) {
            if (matches_at(source_rule, data, at)) {
                report_hit(rules, source_rule, relative, line_of(data, at));
                offenders++;
            }
        }
    }
    return offenders;
}

static int scan_file(const rule_set *rules, const char *root, const char *relative, size_t *offenders) {
    char *path = tree_join(root, relative);
    char *data = NULL;
    size_t size;
    int read = path != NULL && tree_read_file(path, &data, &size);
    if (!read) {
        fprintf(stderr, "cannot read %s/%s\n", root, relative);
    } else {
        *offenders += scan_source(rules, relative, data);
    }
    free(data);
    free(path);
    return read;
}

int core_sources_scan(const rule_set *rules, const char *root) {
    text_list files;
    char error[512];
    size_t offenders = 0;
    size_t sources_read = 0;
    size_t at;
    if (!tree_list_files(root, &files, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 2;
    }
    for (at = 0; at < files.count; at++) {
        if (!is_scanned(files.items[at])) {
            continue;
        }
        if (!scan_file(rules, root, files.items[at], &offenders)) {
            text_list_free(&files);
            return 2;
        }
        sources_read++;
    }
    text_list_free(&files);
    if (sources_read == 0) {
        fprintf(stderr, "no sources under %s/src or %s/include, so nothing was checked\n", root, root);
        return 2;
    }
    printf("%zu sources read under %s\n", sources_read, root);
    if (offenders > 0) {
        fprintf(stderr, "the core's sources read the world: %zu offenders\n", offenders);
        return 1;
    }
    printf("the core's sources read nothing of the world\n");
    return 0;
}
