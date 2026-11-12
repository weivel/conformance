#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "languages.h"
#include "tree.h"

#define DEFAULT_SOURCE_ROOT "src"
#define OPTIONAL_SUFFIX " optional"

typedef struct {
    char *name;
    int optional;
} module_entry;

typedef struct {
    module_entry *items;
    size_t count;
} module_list;

static int parse_modules(const text_list *lines, module_list *modules) {
    size_t at;
    modules->items = calloc(lines->count, sizeof(module_entry));
    modules->count = 0;
    if (modules->items == NULL) {
        return 0;
    }
    for (at = 0; at < lines->count; at++) {
        char *line = lines->items[at];
        char *space = strchr(line, ' ');
        module_entry *entry = &modules->items[modules->count];
        entry->optional = space != NULL && strcmp(space, OPTIONAL_SUFFIX) == 0;
        if (space != NULL) {
            *space = '\0';
        }
        entry->name = line;
        modules->count++;
    }
    return 1;
}

static int is_listed(const module_list *modules, const char *name) {
    size_t at;
    for (at = 0; at < modules->count; at++) {
        if (strcmp(modules->items[at].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_present(const text_list *directories, const char *name) {
    size_t at;
    for (at = 0; at < directories->count; at++) {
        if (strcmp(directories->items[at], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int report_offenders(const module_list *modules, const text_list *directories, const char *language,
                            const char *source_root) {
    size_t at;
    int offenders = 0;
    for (at = 0; at < modules->count; at++) {
        if (!modules->items[at].optional && !is_present(directories, modules->items[at].name)) {
            fprintf(stderr, "%s: missing required module '%s' under %s\n", language, modules->items[at].name,
                    source_root);
            offenders++;
        }
    }
    for (at = 0; at < directories->count; at++) {
        if (!is_listed(modules, directories->items[at])) {
            fprintf(stderr, "%s: '%s' under %s is not a module on the list\n", language, directories->items[at],
                    source_root);
            offenders++;
        }
    }
    return offenders;
}

static char *copy_text(const char *text) {
    char *copy = malloc(strlen(text) + 1);
    if (copy != NULL) {
        memcpy(copy, text, strlen(text) + 1);
    }
    return copy;
}

static char *resolve_source_root(const char *languages_path, const char *language, char *error, size_t error_size) {
    language_rules rules;
    char *source_root;
    if (languages_path == NULL) {
        return copy_text(DEFAULT_SOURCE_ROOT);
    }
    if (!language_rules_load(languages_path, language, &rules, error, error_size)) {
        return NULL;
    }
    source_root = copy_text(rules.source_root != NULL ? rules.source_root : DEFAULT_SOURCE_ROOT);
    language_rules_free(&rules);
    return source_root;
}

static int check_root(const char *root, const module_list *modules, const char *language, const char *source_root,
                      char *error, size_t error_size) {
    char *directory = tree_join(root, source_root);
    text_list directories;
    int offenders;
    if (directory == NULL) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    if (!tree_list_directories(directory, &directories, error, error_size)) {
        free(directory);
        return -1;
    }
    offenders = report_offenders(modules, &directories, language, source_root);
    if (offenders == 0) {
        printf("weivel-parity: %s carries every required module and no other\n", language);
    }
    text_list_free(&directories);
    free(directory);
    return offenders;
}

static int load_modules(const char *path, text_list *lines, module_list *modules) {
    char error[512];
    if (!tree_read_lines(path, lines, error, sizeof error)) {
        fprintf(stderr, "weivel-parity: %s\n", error);
        return 0;
    }
    if (lines->count == 0) {
        fprintf(stderr, "weivel-parity: no modules in %s, so nothing was checked\n", path);
        text_list_free(lines);
        return 0;
    }
    if (!parse_modules(lines, modules)) {
        fprintf(stderr, "weivel-parity: out of memory\n");
        text_list_free(lines);
        return 0;
    }
    return 1;
}

int main(int argument_count, char **arguments) {
    char error[512] = "";
    text_list lines;
    module_list modules;
    char *root;
    char *source_root;
    int offenders;
    if (argument_count < 4 || argument_count > 5) {
        fprintf(stderr, "usage: weivel-parity <root> <modules-file> <language> [languages-file]\n");
        return 2;
    }
    if (!load_modules(arguments[2], &lines, &modules)) {
        return 2;
    }
    source_root = resolve_source_root(argument_count == 5 ? arguments[4] : NULL, arguments[3], error, sizeof error);
    root = tree_normalize_root(arguments[1]);
    if (source_root == NULL || root == NULL) {
        fprintf(stderr, "weivel-parity: %s\n", error[0] != '\0' ? error : "out of memory");
        offenders = -2;
    } else {
        offenders = check_root(root, &modules, arguments[3], source_root, error, sizeof error);
        if (offenders < 0) {
            fprintf(stderr, "weivel-parity: %s\n", error);
        }
    }
    free(modules.items);
    free(source_root);
    free(root);
    text_list_free(&lines);
    return offenders < 0 ? 2 : offenders > 0 ? 1 : 0;
}
