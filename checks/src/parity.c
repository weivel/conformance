#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "languages.h"
#include "tree.h"

#define OPTIONAL_SUFFIX " optional"
#define PINNED_MODULES "siblings/core/MODULES"

typedef struct {
    char *name;
    int optional;
} module_entry;

typedef struct {
    module_entry *items;
    size_t count;
} module_list;

/** A tree a language's modules are spread over: its source root and the source base around it, or no base. */
typedef struct {
    const char *root;
    const char *base;
} source_tree;

typedef struct {
    source_tree *items;
    size_t count;
} source_layout;

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

static int report_module_directories(const module_list *modules, const text_list *directories, const char *language,
                                     const char *source_root, const char *source_directory) {
    size_t at;
    int offenders = 0;
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

/** The source roots joined by " or ", for naming where a missing module was looked for. Owned by the caller. */
static char *join_roots(const source_layout *layout) {
    size_t length = 1;
    size_t at;
    char *joined;
    for (at = 0; at < layout->count; at++) {
        length += strlen(layout->items[at].root) + 4;
    }
    joined = malloc(length);
    if (joined == NULL) {
        return NULL;
    }
    length = 0;
    for (at = 0; at < layout->count; at++) {
        length += (size_t)snprintf(joined + length, 5, "%s", at > 0 ? " or " : "");
        memcpy(joined + length, layout->items[at].root, strlen(layout->items[at].root) + 1);
        length += strlen(layout->items[at].root);
    }
    return joined;
}

static int report_missing_modules(const module_list *modules, const text_list *present, const char *language,
                                  const source_layout *layout, char *error, size_t error_size) {
    char *roots = join_roots(layout);
    size_t at;
    int offenders = 0;
    if (roots == NULL) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    for (at = 0; at < modules->count; at++) {
        if (!modules->items[at].optional && !is_present(present, modules->items[at].name)) {
            fprintf(stderr, "%s: missing required module '%s' under %s\n", language, modules->items[at].name, roots);
            offenders++;
        }
    }
    free(roots);
    return offenders;
}

/** Whether `base` holds `root` strictly: `root` lies below it, never equal to it or beside it under a longer name. */
static int holds_path(const char *base, const char *root) {
    size_t length = strlen(base);
    return strncmp(root, base, length) == 0 && root[length] == '/' && root[length + 1] != '\0';
}

static size_t count_roots_held(const text_list *roots, const char *base) {
    size_t at;
    size_t held = 0;
    for (at = 0; at < roots->count; at++) {
        held += holds_path(base, roots->items[at]);
    }
    return held;
}

/** The one source base holding `root`, NULL when none does, and `*holders` how many do. */
static const char *base_of(const text_list *bases, const char *root, size_t *holders) {
    const char *found = NULL;
    size_t at;
    *holders = 0;
    for (at = 0; at < bases->count; at++) {
        if (holds_path(bases->items[at], root)) {
            found = bases->items[at];
            (*holders)++;
        }
    }
    return found;
}

/** Refuses a base that holds no source root or several; answers 1 when every base holds exactly one. */
static int bases_pair(const char *languages_path, const char *language, const language_rules *rules, char *error,
                      size_t error_size) {
    size_t at;
    for (at = 0; at < rules->source_bases.count; at++) {
        const char *base = rules->source_bases.items[at];
        size_t held = count_roots_held(&rules->source_roots, base);
        if (held == 0) {
            snprintf(error, error_size, "%s gives %s the source-base %s, which does not hold its source-root %s",
                     languages_path, language, base, rules->source_roots.items[0]);
            return 0;
        }
        if (held > 1) {
            snprintf(error, error_size, "%s gives %s the source-base %s, which holds more than one source-root",
                     languages_path, language, base);
            return 0;
        }
    }
    return 1;
}

/**
 * Pairs each source root with the base holding it. Where the language names any base, each root must lie under
 * exactly one, since a root no base holds would leave the files beside it unjudged.
 */
static int pair_roots(const char *languages_path, const char *language, const language_rules *rules,
                      source_layout *layout, char *error, size_t error_size) {
    size_t at;
    if (!bases_pair(languages_path, language, rules, error, error_size)) {
        return 0;
    }
    layout->items = calloc(rules->source_roots.count, sizeof(source_tree));
    layout->count = 0;
    if (layout->items == NULL) {
        snprintf(error, error_size, "out of memory");
        return 0;
    }
    for (at = 0; at < rules->source_roots.count; at++) {
        const char *root = rules->source_roots.items[at];
        size_t holders;
        const char *base = base_of(&rules->source_bases, root, &holders);
        if (rules->source_bases.count > 0 && holders != 1) {
            snprintf(error, error_size, "%s gives %s the source-root %s, which %s", languages_path, language, root,
                     holders == 0 ? "no source-base holds" : "more than one source-base holds");
            free(layout->items);
            layout->items = NULL;
            return 0;
        }
        layout->items[layout->count].root = root;
        layout->items[layout->count].base = base;
        layout->count++;
    }
    return 1;
}

static int describe_layout_refusal(const char *languages_path, const char *language, const language_rules *rules,
                                   char *error, size_t error_size) {
    if (!rules->known) {
        snprintf(error, error_size, "%s names no language '%s', so its module layout is unknown", languages_path,
                 language);
    } else if (rules->source_roots.count == 0) {
        snprintf(error, error_size, "%s gives %s no source-root, so its modules have nowhere to be found",
                 languages_path, language);
    } else if (rules->header_root != NULL && rules->extensions.count == 0) {
        snprintf(error, error_size, "%s gives %s a header-root and no extension, so none of its headers can be told",
                 languages_path, language);
    } else {
        return 0;
    }
    return 1;
}

static int load_layout(const char *languages_path, const char *language, language_rules *rules,
                       source_layout *layout, char *error, size_t error_size) {
    if (!language_rules_load(languages_path, language, rules, error, error_size)) {
        return 0;
    }
    if (!describe_layout_refusal(languages_path, language, rules, error, error_size) &&
        pair_roots(languages_path, language, rules, layout, error, error_size)) {
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

static int add_every(text_list *present, const text_list *directories) {
    size_t at;
    for (at = 0; at < directories->count; at++) {
        if (!is_present(present, directories->items[at]) && !text_list_add(present, directories->items[at])) {
            return 0;
        }
    }
    return 1;
}

/**
 * Judges the module directories and loose files under one source root and adds the directories found to `present`,
 * or answers -1 with `error` filled when the root cannot be read.
 */
static int count_tree_offenders(const char *root, const module_list *modules, const char *language,
                                const char *source_root, text_list *present, char *error, size_t error_size) {
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
        offenders += report_module_directories(modules, &directories, language, source_root, directory);
    }
    if (offenders >= 0 && !add_every(present, &directories)) {
        snprintf(error, error_size, "out of memory");
        offenders = -1;
    }
    text_list_free(&directories);
    free(directory);
    return offenders;
}

/** Judges every source root, then the required modules against all of them: a module present under any one counts. */
static int count_source_offenders(const char *root, const module_list *modules, const char *language,
                                  const source_layout *layout, char *error, size_t error_size) {
    text_list present = {NULL, 0};
    size_t at;
    int offenders = 0;
    for (at = 0; offenders >= 0 && at < layout->count; at++) {
        int found = count_tree_offenders(root, modules, language, layout->items[at].root, &present, error, error_size);
        offenders = found < 0 ? -1 : offenders + found;
    }
    if (offenders >= 0) {
        int missing = report_missing_modules(modules, &present, language, layout, error, error_size);
        offenders = missing < 0 ? -1 : offenders + missing;
    }
    text_list_free(&present);
    return offenders;
}

/**
 * Counts the files under the tree's source base that lie outside its source root, or answers -1 with `error` filled
 * when the source base cannot be read. Nothing under the base is skipped by name, since every file there is judged.
 */
static int report_files_outside_the_root(const char *root, const char *language, const source_tree *tree,
                                         char *error, size_t error_size) {
    char *directory = tree_join(root, tree->base);
    const char *inner_root = tree->root + strlen(tree->base) + 1;
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
                    tree->base, tree->root);
            offenders++;
        }
    }
    text_list_free(&files);
    return offenders;
}

/**
 * Counts the directories directly inside the level of the tree's source root that ends at `level_end` other than the
 * root's next step, which starts at `step` and runs `step_length` bytes, and sets `step_present`. Answers -1 with
 * `error` filled when the level cannot be read.
 */
static int report_one_level(const char *root, const char *language, const source_tree *tree, size_t level_end,
                            const char *step, size_t step_length, int *step_present, char *error, size_t error_size) {
    size_t base_length = strlen(tree->base);
    const char *walked = tree->root + base_length + 1;
    int walked_length = level_end > base_length ? (int)(level_end - base_length - 1) : 0;
    char *level = malloc(level_end + 1);
    char *directory;
    text_list names;
    size_t at;
    int offenders = 0;
    if (level == NULL) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    memcpy(level, tree->root, level_end);
    level[level_end] = '\0';
    directory = tree_join(root, level);
    free(level);
    if (directory == NULL) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    if (!tree_list_directories(directory, &names, error, error_size)) {
        free(directory);
        return -1;
    }
    free(directory);
    *step_present = 0;
    for (at = 0; at < names.count; at++) {
        if (strlen(names.items[at]) == step_length && strncmp(names.items[at], step, step_length) == 0) {
            *step_present = 1;
        } else {
            fprintf(stderr, "%s: '%.*s%s%s' under %s is a directory outside the source root %s\n", language,
                    walked_length, walked, walked_length > 0 ? "/" : "", names.items[at], tree->base, tree->root);
            offenders++;
        }
    }
    text_list_free(&names);
    return offenders;
}

/**
 * Counts the directories met on the way from the tree's source base down to its source root that are not the root's
 * next step, or answers -1 with `error` filled when one cannot be read.
 *
 * @implNote The file walk cannot see a directory that holds no file, so each level is judged by its directory names
 * as well; the walk stops at a step that is absent, since a missing root is reported by the module check.
 */
static int report_directories_beside_the_root(const char *root, const char *language, const source_tree *tree,
                                              char *error, size_t error_size) {
    size_t level_end = strlen(tree->base);
    int offenders = 0;
    int step_present = 1;
    while (step_present && tree->root[level_end] == '/' && strcspn(tree->root + level_end + 1, "/") > 0) {
        const char *step = tree->root + level_end + 1;
        size_t step_length = strcspn(step, "/");
        int found =
            report_one_level(root, language, tree, level_end, step, step_length, &step_present, error, error_size);
        if (found < 0) {
            return -1;
        }
        offenders += found;
        level_end += 1 + step_length;
    }
    return offenders;
}

typedef int (*tree_judge)(const char *root, const char *language, const source_tree *tree, char *error,
                          size_t error_size);

static int judge_every_base(const char *root, const char *language, const source_layout *layout, tree_judge judge,
                            char *error, size_t error_size) {
    size_t at;
    int offenders = 0;
    for (at = 0; offenders >= 0 && at < layout->count; at++) {
        if (layout->items[at].base != NULL) {
            int found = judge(root, language, &layout->items[at], error, error_size);
            offenders = found < 0 ? -1 : offenders + found;
        }
    }
    return offenders;
}

static int check_root(const char *root, const module_list *modules, const char *language,
                      const language_rules *rules, const source_layout *layout, char *error, size_t error_size) {
    int offenders = count_source_offenders(root, modules, language, layout, error, error_size);
    if (offenders >= 0) {
        int outside = judge_every_base(root, language, layout, report_files_outside_the_root, error, error_size);
        offenders = outside < 0 ? -1 : offenders + outside;
    }
    if (offenders >= 0) {
        int beside = judge_every_base(root, language, layout, report_directories_beside_the_root, error, error_size);
        offenders = beside < 0 ? -1 : offenders + beside;
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

/**
 * Reads the module list of the core the repository pins, at `siblings/core/MODULES` under it.
 *
 * @implNote Never the kit's own core: a language pinned to an older core than the kit's would be judged against a
 * list it has not adopted yet, and the dependency checkout always puts the core at `siblings/core`.
 */
static int load_pinned_modules(const char *root, text_list *lines, module_list *modules) {
    char error[512];
    char *path = tree_join(root, PINNED_MODULES);
    if (path == NULL || !tree_read_lines(path, lines, error, sizeof error)) {
        fprintf(stderr,
                "weivel-parity: no %s under %s, so the modules its core lists are unknown; check out its pinned "
                "dependencies first, the core at siblings/core\n",
                PINNED_MODULES, root);
        free(path);
        return 0;
    }
    if (lines->count == 0) {
        fprintf(stderr, "weivel-parity: no modules in %s, so nothing was checked\n", path);
        text_list_free(lines);
        free(path);
        return 0;
    }
    free(path);
    if (!parse_modules(lines, modules)) {
        fprintf(stderr, "weivel-parity: out of memory\n");
        text_list_free(lines);
        return 0;
    }
    return 1;
}

static int check_repository(const char *root, const char *language, const char *languages_path) {
    char error[512] = "";
    text_list lines;
    module_list modules;
    language_rules rules;
    source_layout layout = {NULL, 0};
    int offenders = -1;
    if (!load_pinned_modules(root, &lines, &modules)) {
        return -1;
    }
    if (load_layout(languages_path, language, &rules, &layout, error, sizeof error)) {
        offenders = check_root(root, &modules, language, &rules, &layout, error, sizeof error);
        free(layout.items);
        language_rules_free(&rules);
    }
    if (offenders < 0) {
        fprintf(stderr, "weivel-parity: %s\n", error[0] != '\0' ? error : "out of memory");
    }
    free(modules.items);
    text_list_free(&lines);
    return offenders;
}

int main(int argument_count, char **arguments) {
    char *root;
    int offenders = -1;
    if (argument_count != 4) {
        fprintf(stderr, "usage: weivel-parity <root> <language> <languages-file>\n");
        return 2;
    }
    root = tree_normalize_root(arguments[1]);
    if (root == NULL) {
        fprintf(stderr, "weivel-parity: out of memory\n");
        return 2;
    }
    offenders = check_repository(root, arguments[2], arguments[3]);
    free(root);
    return offenders < 0 ? 2 : offenders > 0 ? 1 : 0;
}
