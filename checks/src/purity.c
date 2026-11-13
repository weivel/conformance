#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "languages.h"
#include "tree.h"

static const char *const COMMON_EXTENSIONS[] = {".md", ".txt"};
static const char *const COMMON_NAMES[] = {".gitignore", ".gitattributes", "LICENSE", "MODULES"};
static const char *const COMMON_PREFIX = ".weivel/";
static const char *const NATIVE_LANGUAGE = "c";
static const char *const NATIVE_DATA_EXTENSION = ".json";
static const char *const NATIVE_ROLES[] = {"c", "core", "kit", "standard"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static int has_suffix(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length > suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int matches_pattern(const char *name, const char *pattern) {
    const char *star = strchr(pattern, '*');
    size_t prefix_length;
    size_t suffix_length;
    size_t name_length = strlen(name);
    if (star == NULL) {
        return strcmp(name, pattern) == 0;
    }
    prefix_length = (size_t)(star - pattern);
    suffix_length = strlen(star + 1);
    return name_length >= prefix_length + suffix_length && strncmp(name, pattern, prefix_length) == 0 &&
           strcmp(name + name_length - suffix_length, star + 1) == 0;
}

static int in_list_by_suffix(const text_list *list, const char *name) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        if (has_suffix(name, list->items[at])) {
            return 1;
        }
    }
    return 0;
}

static int in_list_by_equality(const text_list *list, const char *name) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        if (strcmp(name, list->items[at]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int in_list_by_pattern(const text_list *list, const char *name) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        if (matches_pattern(name, list->items[at])) {
            return 1;
        }
    }
    return 0;
}

static int in_list_by_prefix(const text_list *list, const char *path) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        if (strncmp(path, list->items[at], strlen(list->items[at])) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_common(const char *path) {
    const char *name = base_name(path);
    size_t at;
    for (at = 0; at < COUNT_OF(COMMON_EXTENSIONS); at++) {
        if (has_suffix(name, COMMON_EXTENSIONS[at])) {
            return 1;
        }
    }
    for (at = 0; at < COUNT_OF(COMMON_NAMES); at++) {
        if (strcmp(name, COMMON_NAMES[at]) == 0 && strcmp(path, name) == 0) {
            return 1;
        }
    }
    return strncmp(path, COMMON_PREFIX, strlen(COMMON_PREFIX)) == 0;
}

static int is_native_role(const char *role) {
    size_t at;
    for (at = 0; at < COUNT_OF(NATIVE_ROLES); at++) {
        if (strcmp(role, NATIVE_ROLES[at]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_language_file(const language_rules *rules, const char *path) {
    const char *name = base_name(path);
    return in_list_by_suffix(&rules->extensions, name) || in_list_by_equality(&rules->names, name) ||
           in_list_by_pattern(&rules->patterns, name) || in_list_by_prefix(&rules->prefixes, path);
}

/** Whether a native role allows `path`: its language or a data file, and never another language's build file. */
static int is_native(const char *path, const language_rules *native_language,
                     const language_rules *other_languages) {
    const char *name = base_name(path);
    if (in_list_by_equality(&other_languages->names, name) || in_list_by_pattern(&other_languages->patterns, name)) {
        return 0;
    }
    return is_language_file(native_language, path) || has_suffix(name, NATIVE_DATA_EXTENSION);
}

static int report_offenders(const text_list *files, int native, const language_rules *rules,
                            const language_rules *other_languages, const char *role) {
    size_t at;
    int offenders = 0;
    for (at = 0; at < files->count; at++) {
        const char *path = files->items[at];
        int allowed = is_common(path) ||
                      (native ? is_native(path, rules, other_languages) : is_language_file(rules, path));
        if (!allowed) {
            fprintf(stderr, "%s: not allowed in a repository with role '%s'\n", path, role);
            offenders++;
        }
    }
    return offenders;
}

/**
 * Loads the rules of the language the role is written in, which is the native language for a native role, and for a
 * native role the rules of every other language, whose build files it refuses.
 */
static int load_rules(const char *languages_path, const char *role, int native, language_rules *rules,
                      language_rules *other_languages) {
    char error[512];
    const char *language = native ? NATIVE_LANGUAGE : role;
    memset(other_languages, 0, sizeof *other_languages);
    if (!language_rules_load(languages_path, language, rules, error, sizeof error)) {
        fprintf(stderr, "weivel-purity: %s\n", error);
        return 0;
    }
    if (!rules->known) {
        if (native) {
            fprintf(stderr, "weivel-purity: %s lists no language '%s', which the native role '%s' is written in\n",
                    languages_path, language, role);
        } else {
            fprintf(stderr, "weivel-purity: role '%s' is neither a native role nor listed in %s\n", role,
                    languages_path);
        }
        language_rules_free(rules);
        return 0;
    }
    if (native && !language_rules_load_others(languages_path, language, other_languages, error, sizeof error)) {
        fprintf(stderr, "weivel-purity: %s\n", error);
        language_rules_free(rules);
        return 0;
    }
    return 1;
}

int main(int argument_count, char **arguments) {
    char error[512];
    text_list files;
    language_rules rules;
    language_rules other_languages;
    const char *role;
    char *root;
    int native;
    int offenders;
    if (argument_count != 4) {
        fprintf(stderr, "usage: weivel-purity <root> <role> <languages-file>\n");
        return 2;
    }
    role = arguments[2];
    native = is_native_role(role);
    if (!load_rules(arguments[3], role, native, &rules, &other_languages)) {
        return 2;
    }
    root = tree_normalize_root(arguments[1]);
    if (root == NULL || !tree_list_files(root, &files, error, sizeof error)) {
        fprintf(stderr, "weivel-purity: %s\n", root == NULL ? "out of memory" : error);
        offenders = -1;
    } else if (files.count == 0) {
        fprintf(stderr, "weivel-purity: no files under %s, so nothing was checked\n", root);
        offenders = -1;
        text_list_free(&files);
    } else {
        offenders = report_offenders(&files, native, &rules, &other_languages, role);
        if (offenders == 0) {
            printf("weivel-purity: %zu files, all allowed for role '%s'\n", files.count, role);
        }
        text_list_free(&files);
    }
    language_rules_free(&rules);
    language_rules_free(&other_languages);
    free(root);
    return offenders < 0 ? 2 : offenders > 0 ? 1 : 0;
}
