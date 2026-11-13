#include "lockfile.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "weivel/json.h"

#define WHERE_CAPACITY 512
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const char LOCKFILE_NAME[] = "package-lock.json";
static const char *const DEPENDENCY_MAPS[] = {"dependencies", "devDependencies", "optionalDependencies",
                                              "peerDependencies", "peerDependenciesMeta", "requires"};
static const char *const ENTRY_TEXTS[] = {"name", "version", "resolved"};

typedef struct {
    const char *relative;
    const text_list *words;
    int offenders;
} lockfile_scan;

int lockfile_is_lockfile(const char *relative) {
    const char *slash = strrchr(relative, '/');
    return strcmp(slash == NULL ? relative : slash + 1, LOCKFILE_NAME) == 0;
}

static char *folded_copy(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    size_t at;
    if (copy == NULL) {
        return NULL;
    }
    for (at = 0; at <= length; at++) {
        copy[at] = (char)tolower((unsigned char)text[at]);
    }
    return copy;
}

static void report_text(lockfile_scan *scan, const char *where, const char *text) {
    char *folded = folded_copy(text);
    size_t at;
    if (folded == NULL) {
        fprintf(stderr, "%s: %s: out of memory\n", scan->relative, where);
        scan->offenders++;
        return;
    }
    for (at = 0; at < scan->words->count; at++) {
        if (strstr(folded, scan->words->items[at]) != NULL) {
            fprintf(stderr, "%s: %s names '%s'\n", scan->relative, where, scan->words->items[at]);
            scan->offenders++;
        }
    }
    free(folded);
}

static void mark_cut(char *member, int written) {
    if (written < 0 || written >= WHERE_CAPACITY) {
        memcpy(member + WHERE_CAPACITY - 4, "...", 4);
    }
}

static void describe_key(char *member, const char *parent, const char *key) {
    mark_cut(member, snprintf(member, WHERE_CAPACITY, "%s[\"%s\"]", parent, key));
}

static void describe_field(char *member, const char *parent, const char *field) {
    mark_cut(member, snprintf(member, WHERE_CAPACITY, "%s.%s", parent, field));
}

static void scan_entry(lockfile_scan *scan, const char *where, const weivel_json_value *entry);

static void scan_dependency_map(lockfile_scan *scan, const char *where, const weivel_json_value *map) {
    size_t count = weivel_json_object_size(map);
    size_t at;
    for (at = 0; at < count; at++) {
        const char *key = weivel_json_object_key_at(map, at);
        const weivel_json_value *value = weivel_json_object_value_at(map, at);
        char member[WHERE_CAPACITY];
        describe_key(member, where, key);
        report_text(scan, member, key);
        if (weivel_json_kind_of(value) == WEIVEL_JSON_STRING) {
            report_text(scan, member, weivel_json_as_string(value));
        } else if (weivel_json_kind_of(value) == WEIVEL_JSON_OBJECT) {
            scan_entry(scan, member, value);
        }
    }
}

static void scan_entry(lockfile_scan *scan, const char *where, const weivel_json_value *entry) {
    char member[WHERE_CAPACITY];
    size_t at;
    for (at = 0; at < COUNT_OF(ENTRY_TEXTS); at++) {
        const char *text = weivel_json_as_string(weivel_json_object_get(entry, ENTRY_TEXTS[at]));
        if (text != NULL) {
            describe_field(member, where, ENTRY_TEXTS[at]);
            report_text(scan, member, text);
        }
    }
    for (at = 0; at < COUNT_OF(DEPENDENCY_MAPS); at++) {
        describe_field(member, where, DEPENDENCY_MAPS[at]);
        scan_dependency_map(scan, member, weivel_json_object_get(entry, DEPENDENCY_MAPS[at]));
    }
}

static void scan_packages(lockfile_scan *scan, const weivel_json_value *packages) {
    size_t count = weivel_json_object_size(packages);
    size_t at;
    for (at = 0; at < count; at++) {
        const char *key = weivel_json_object_key_at(packages, at);
        char member[WHERE_CAPACITY];
        describe_key(member, "packages", key);
        report_text(scan, member, key);
        scan_entry(scan, member, weivel_json_object_value_at(packages, at));
    }
}

int lockfile_report_words(const char *relative, const char *data, size_t size, const text_list *words) {
    weivel_json_error error = {sizeof(weivel_json_error), 0, 0, NULL, {0}, 0};
    weivel_json_value *root = weivel_json_parse(data, size, &error);
    lockfile_scan scan = {relative, words, 0};
    if (root == NULL || weivel_json_kind_of(root) != WEIVEL_JSON_OBJECT) {
        if (root == NULL) {
            fprintf(stderr, "%s:%d:%d: cannot be read as a lockfile: %s\n", relative, error.line, error.column,
                    error.message == NULL ? "unreadable" : error.message);
        } else {
            fprintf(stderr, "%s: cannot be read as a lockfile: the top level is not an object\n", relative);
        }
        weivel_json_free(root);
        return 1;
    }
    scan_entry(&scan, "lockfile", root);
    scan_packages(&scan, weivel_json_object_get(root, "packages"));
    weivel_json_free(root);
    return scan.offenders;
}
