#include "core_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

static const char *const RULE_KIND_NAMES[] = {"allow-everywhere", "allow-in-mutex", "allow-from-compiler",
    "allow-from-startup", "startup-export", "forbid", "forbid-name", "forbid-source", "source-in-mutex-only"};
static const char *const RULE_FORM_NAMES[] = {"symbol", "word", "call", "prefix", "text"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define MUTEX_MARKER_KIND "mutex-object-defines"
#define MUTEX_SOURCE_KIND "mutex-source"

char *duplicate_text(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

size_t split_tokens(char *line, char **tokens) {
    size_t count = 0;
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        if (count < MAXIMUM_TOKENS) {
            tokens[count] = cursor;
        }
        count++;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

static int contains_ignoring_case(const char *text, const char *needle, size_t needle_length) {
    size_t text_length = strlen(text);
    size_t start;
    size_t at;
    for (start = 0; start + needle_length <= text_length; start++) {
        for (at = 0; at < needle_length; at++) {
            if (tolower((unsigned char)text[start + at]) != tolower((unsigned char)needle[at])) {
                break;
            }
        }
        if (at == needle_length) {
            return 1;
        }
    }
    return 0;
}

static int pattern_matches(const char *pattern, const char *symbol) {
    size_t length = strlen(pattern);
    if (length >= 2 && pattern[0] == '*' && pattern[length - 1] == '*') {
        return contains_ignoring_case(symbol, pattern + 1, length - 2);
    }
    if (length >= 1 && pattern[length - 1] == '*') {
        return strncmp(symbol, pattern, length - 1) == 0;
    }
    return strcmp(symbol, pattern) == 0;
}

const rule *rules_find(const rule_set *rules, rule_kind kind, const char *symbol) {
    size_t at;
    for (at = 0; at < rules->count; at++) {
        if (rules->items[at].kind == kind && pattern_matches(rules->items[at].pattern, symbol)) {
            return &rules->items[at];
        }
    }
    return NULL;
}

size_t rules_count(const rule_set *rules, rule_kind kind) {
    size_t at;
    size_t count = 0;
    for (at = 0; at < rules->count; at++) {
        count += rules->items[at].kind == kind;
    }
    return count;
}

static int find_name(const char *const *names, size_t count, const char *name, size_t *found) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (strcmp(name, names[at]) == 0) {
            *found = at;
            return 1;
        }
    }
    return 0;
}

static int is_source_kind(rule_kind kind) {
    return kind == RULE_FORBID_SOURCE || kind == RULE_SOURCE_IN_MUTEX_ONLY;
}

static int add_rule(rule_set *rules, rule_kind kind, rule_form form, const char *reason, const char *pattern) {
    rule *grown = realloc(rules->items, (rules->count + 1) * sizeof *grown);
    rule *added;
    if (grown == NULL) {
        return 0;
    }
    rules->items = grown;
    added = &grown[rules->count++];
    added->kind = kind;
    added->form = form;
    added->reason = duplicate_text(reason, strlen(reason));
    added->pattern = duplicate_text(pattern, strlen(pattern));
    return added->reason != NULL && added->pattern != NULL;
}

static int set_single(char **slot, const char *value) {
    free(*slot);
    *slot = duplicate_text(value, strlen(value));
    return *slot != NULL;
}

static int parse_rule_line(char *line, rule_set *rules, const char *path) {
    char *tokens[MAXIMUM_TOKENS];
    size_t count = split_tokens(line, tokens);
    size_t kind;
    size_t form = FORM_SYMBOL;
    if (count == 2 && strcmp(tokens[0], MUTEX_MARKER_KIND) == 0) {
        return set_single(&rules->mutex_marker, tokens[1]);
    }
    if (count == 2 && strcmp(tokens[0], MUTEX_SOURCE_KIND) == 0) {
        return set_single(&rules->mutex_source, tokens[1]);
    }
    if (!find_name(RULE_KIND_NAMES, COUNT_OF(RULE_KIND_NAMES), tokens[0], &kind) ||
        count != (is_source_kind((rule_kind)kind) ? 4u : 3u) ||
        (count == 4 && (!find_name(RULE_FORM_NAMES, COUNT_OF(RULE_FORM_NAMES), tokens[2], &form) ||
                           form == FORM_SYMBOL))) {
        fprintf(stderr, "%s: cannot read the rule '%s'\n", path, tokens[0]);
        return 0;
    }
    return add_rule(rules, (rule_kind)kind, (rule_form)form, tokens[1], tokens[count - 1]);
}

int rules_load(const char *path, rule_set *rules) {
    text_list lines;
    char error[512];
    size_t at;
    int ok = 1;
    if (!tree_read_lines(path, &lines, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 0;
    }
    for (at = 0; ok && at < lines.count; at++) {
        ok = parse_rule_line(lines.items[at], rules, path);
    }
    text_list_free(&lines);
    return ok;
}

void rules_free(rule_set *rules) {
    size_t at;
    for (at = 0; at < rules->count; at++) {
        free(rules->items[at].reason);
        free(rules->items[at].pattern);
    }
    free(rules->items);
    free(rules->mutex_marker);
    free(rules->mutex_source);
}
