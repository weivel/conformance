#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "languages.h"
#include "tree.h"

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

/**
 * Whether a module directory holds a file at any depth.
 *
 * @implNote A directory directly inside the module that a repository walk skips (`build`, `node_modules` and the
 * rest) is skipped here too, so a module holding nothing but build output still holds no file.
 */
static int holds_a_file(const char *source_directory, const char *name) {
    char error[512];
    text_list files;
    char *directory = tree_join(source_directory, name);
    int holds = directory != NULL && tree_list_files(directory, &files, error, sizeof error) && files.count > 0;
    if (directory != NULL && holds) {
        text_list_free(&files);
    }
    free(directory);
    return holds;
}

static int report_offenders(const module_list *modules, const text_list *directories, const char *language,
                            const char *source_root, const char *source_directory) {
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
        } else if (!holds_a_file(source_directory, directories->items[at])) {
            fprintf(stderr, "%s: module '%s' under %s holds no file\n", language, directories->items[at],
                    source_root);
            offenders++;
        }
    }
    return offenders;
}

/** Whether `base` holds `root` strictly: `root` lies below it, never equal to it or beside it under a longer name. */
static int holds_path(const char *base, const char *root) {
    size_t length = strlen(base);
    return strncmp(root, base, length) == 0 && root[length] == '/' && root[length + 1] != '\0';
}

static int load_layout(const char *languages_path, const char *language, language_rules *rules, char *error,
                       size_t error_size) {
    if (!language_rules_load(languages_path, language, rules, error, error_size)) {
        return 0;
    }
    if (!rules->known) {
        snprintf(error, error_size, "%s names no language '%s', so its module layout is unknown", languages_path,
                 language);
    } else if (rules->source_root == NULL) {
        snprintf(error, error_size, "%s gives %s no source-root, so its modules have nowhere to be found",
                 languages_path, language);
    } else if (rules->source_base != NULL && !holds_path(rules->source_base, rules->source_root)) {
        snprintf(error, error_size, "%s gives %s the source-base %s, which does not hold its source-root %s",
                 languages_path, language, rules->source_base, rules->source_root);
    } else if (rules->header_root != NULL && rules->extensions.count == 0) {
        snprintf(error, error_size, "%s gives %s a header-root and no extension, so none of its headers can be told",
                 languages_path, language);
    } else {
        return 1;
    }
    language_rules_free(rules);
    return 0;
}

static int is_loose_file(const char *relative) {
    return strchr(relative, '/') == NULL;
}

static int report_loose_files(const char *source_directory, const char *language, const char *source_root, char *error,
                              size_t error_size) {
    text_list files;
    size_t at;
    int offenders = 0;
    if (!tree_list_every_file(source_directory, &files, error, error_size)) {
        return -1;
    }
    for (at = 0; at < files.count; at++) {
        if (is_loose_file(files.items[at])) {
            fprintf(stderr, "%s: '%s' under %s is a loose file, outside every module\n", language, files.items[at],
                    source_root);
            offenders++;
        }
    }
    text_list_free(&files);
    return offenders;
}

/** The module a published header belongs to: the directory it sits in, or its own name without the extension. */
static int names_a_module(const module_list *modules, const char *relative) {
    const char *slash = strchr(relative, '/');
    const char *end = slash != NULL ? slash : strrchr(relative, '.');
    size_t length = end != NULL ? (size_t)(end - relative) : strlen(relative);
    size_t at;
    for (at = 0; at < modules->count; at++) {
        if (strlen(modules->items[at].name) == length && strncmp(modules->items[at].name, relative, length) == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *file_name(const char *relative) {
    const char *slash = strrchr(relative, '/');
    return slash == NULL ? relative : slash + 1;
}

/** Whether the file's name ends in one of the language's extensions and holds something before it. */
static int carries_an_extension(const language_rules *rules, const char *relative) {
    const char *name = file_name(relative);
    size_t length = strlen(name);
    size_t at;
    for (at = 0; at < rules->extensions.count; at++) {
        const char *extension = rules->extensions.items[at];
        size_t extension_length = strlen(extension);
        if (length > extension_length && strcmp(name + length - extension_length, extension) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_stray_header(const module_list *modules, const language_rules *rules, const char *language,
                           const char *relative) {
    if (!carries_an_extension(rules, relative)) {
        fprintf(stderr, "%s: '%s' under %s carries none of the language's extensions\n", language, relative,
                rules->header_root);
        return 1;
    }
    if (!names_a_module(modules, relative)) {
        fprintf(stderr, "%s: '%s' under %s belongs to no module on the list\n", language, relative,
                rules->header_root);
        return 1;
    }
    return 0;
}

/**
 * Counts the files under the header root that are no header of a listed module, or answers -1 with `error` filled
 * when the header root cannot be read or holds no file, since nothing was then checked.
 */
static int report_stray_headers(const char *root, const module_list *modules, const char *language,
                                const language_rules *rules, char *error, size_t error_size) {
    char *directory = tree_join(root, rules->header_root);
    text_list files;
    size_t at;
    int offenders = 0;
    if (directory == NULL) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    if (!tree_list_every_file(directory, &files, error, error_size)) {
        free(directory);
        return -1;
    }
    free(directory);
    if (files.count == 0) {
        snprintf(error, error_size, "no file under %s, so nothing was checked", rules->header_root);
        return -1;
    }
    for (at = 0; at < files.count; at++) {
        offenders += is_stray_header(modules, rules, language, files.items[at]);
    }
    text_list_free(&files);
    return offenders;
}

static int count_source_offenders(const char *root, const module_list *modules, const char *language,
                                  const char *source_root, char *error, size_t error_size) {
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
    offenders = report_loose_files(directory, language, source_root, error, error_size);
    if (offenders >= 0) {
        offenders += report_offenders(modules, &directories, language, source_root, directory);
    }
    text_list_free(&directories);
    free(directory);
    return offenders;
}

/**
 * Counts the files under the source base that lie outside the source root, or answers -1 with `error` filled when the
 * source base cannot be read. Nothing under the base is skipped by name, since every file there is judged.
 */
static int report_files_outside_the_root(const char *root, const char *language, const language_rules *rules,
                                         char *error, size_t error_size) {
    char *directory = tree_join(root, rules->source_base);
    const char *inner_root = rules->source_root + strlen(rules->source_base) + 1;
    text_list files;
    size_t at;
    int offenders = 0;
    if (directory == NULL) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    if (!tree_list_every_file(directory, &files, error, error_size)) {
        free(directory);
        return -1;
    }
    free(directory);
    for (at = 0; at < files.count; at++) {
        if (!holds_path(inner_root, files.items[at])) {
            fprintf(stderr, "%s: '%s' under %s lies outside the source root %s\n", language, files.items[at],
                    rules->source_base, rules->source_root);
            offenders++;
        }
    }
    text_list_free(&files);
    return offenders;
}

static int check_root(const char *root, const module_list *modules, const char *language,
                      const language_rules *rules, char *error, size_t error_size) {
    int offenders = count_source_offenders(root, modules, language, rules->source_root, error, error_size);
    if (offenders >= 0 && rules->source_base != NULL) {
        int outside = report_files_outside_the_root(root, language, rules, error, error_size);
        offenders = outside < 0 ? -1 : offenders + outside;
    }
    if (offenders >= 0 && rules->header_root != NULL) {
        int stray = report_stray_headers(root, modules, language, rules, error, error_size);
        offenders = stray < 0 ? -1 : offenders + stray;
    }
    if (offenders == 0) {
        printf("weivel-parity: %s carries every required module and no other\n", language);
    }
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
    language_rules rules;
    char *root;
    int offenders = -1;
    if (argument_count != 5) {
        fprintf(stderr, "usage: weivel-parity <root> <modules-file> <language> <languages-file>\n");
        return 2;
    }
    if (!load_modules(arguments[2], &lines, &modules)) {
        return 2;
    }
    root = tree_normalize_root(arguments[1]);
    if (load_layout(arguments[4], arguments[3], &rules, error, sizeof error)) {
        if (root != NULL) {
            offenders = check_root(root, &modules, arguments[3], &rules, error, sizeof error);
        }
        language_rules_free(&rules);
    }
    if (offenders < 0) {
        fprintf(stderr, "weivel-parity: %s\n", error[0] != '\0' ? error : "out of memory");
    }
    free(modules.items);
    free(root);
    text_list_free(&lines);
    return offenders < 0 ? 2 : offenders > 0 ? 1 : 0;
}
