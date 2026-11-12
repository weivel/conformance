#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tool.h"
#include "tree.h"

#define MAXIMUM_TOKENS 8

typedef enum { FORMAT_MSVC, FORMAT_GNU, FORMAT_APPLE } symbol_format;

typedef enum {
    RULE_ALLOW_EVERYWHERE,
    RULE_ALLOW_IN_MUTEX,
    RULE_ALLOW_FROM_COMPILER,
    RULE_ALLOW_FROM_STARTUP,
    RULE_FORBID,
    RULE_FORBID_NAME
} rule_kind;

static const char *const RULE_KIND_NAMES[] = {
    "allow-everywhere", "allow-in-mutex", "allow-from-compiler", "allow-from-startup", "forbid", "forbid-name"};

#define RULE_KIND_COUNT (sizeof(RULE_KIND_NAMES) / sizeof(RULE_KIND_NAMES[0]))
#define MUTEX_MARKER_KIND "mutex-object-defines"

typedef struct {
    rule_kind kind;
    char *reason;
    char *pattern;
} rule;

typedef struct {
    rule *items;
    size_t count;
    char *mutex_marker;
} rule_set;

typedef struct {
    char *label;
    text_list defined;
    text_list undefined;
    text_list named;
} object_symbols;

typedef struct {
    object_symbols *items;
    size_t count;
} object_list;

typedef struct {
    text_list imports;
    text_list import_sources;
    text_list exports;
    text_list unreadable;
} shared_symbols;

typedef struct {
    const rule_set *rules;
    const object_list *objects;
    const object_symbols *mutex_object;
    size_t offenders;
} verdict;

static char *duplicate(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

static size_t split_tokens(char *line, char **tokens) {
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

static char *next_line(char **cursor) {
    char *line = *cursor;
    char *end;
    size_t length;
    if (*line == '\0') {
        return NULL;
    }
    end = strchr(line, '\n');
    if (end != NULL) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = line + strlen(line);
    }
    length = strlen(line);
    while (length > 0 && isspace((unsigned char)line[length - 1])) {
        line[--length] = '\0';
    }
    return line;
}

static const char *skip_spaces(const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static int ends_with_ignoring_case(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    size_t at;
    if (suffix_length > text_length) {
        return 0;
    }
    for (at = 0; at < suffix_length; at++) {
        if (tolower((unsigned char)text[text_length - suffix_length + at]) != tolower((unsigned char)suffix[at])) {
            return 0;
        }
    }
    return 1;
}

static int is_hex(const char *token) {
    if (*token == '\0') {
        return 0;
    }
    for (; *token != '\0'; token++) {
        if (!isxdigit((unsigned char)*token)) {
            return 0;
        }
    }
    return 1;
}

static int is_decimal(const char *token) {
    if (*token == '\0') {
        return 0;
    }
    for (; *token != '\0'; token++) {
        if (!isdigit((unsigned char)*token)) {
            return 0;
        }
    }
    return 1;
}

static const char *base_name(const char *path) {
    const char *base = path;
    const char *cursor;
    for (cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
        }
    }
    return base;
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

/** `*text*` matches a substring in any case, `text*` a prefix, anything else the whole symbol. */
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

static const rule *find_rule(const rule_set *rules, rule_kind kind, const char *symbol) {
    size_t at;
    for (at = 0; at < rules->count; at++) {
        if (rules->items[at].kind == kind && pattern_matches(rules->items[at].pattern, symbol)) {
            return &rules->items[at];
        }
    }
    return NULL;
}

static int list_contains(const text_list *list, const char *text) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        if (strcmp(list->items[at], text) == 0) {
            return 1;
        }
    }
    return 0;
}

static int parse_rule_kind(const char *name, rule_kind *kind) {
    size_t at;
    for (at = 0; at < RULE_KIND_COUNT; at++) {
        if (strcmp(name, RULE_KIND_NAMES[at]) == 0) {
            *kind = (rule_kind)at;
            return 1;
        }
    }
    return 0;
}

static int add_rule(rule_set *rules, rule_kind kind, const char *reason, const char *pattern) {
    rule *grown = realloc(rules->items, (rules->count + 1) * sizeof *grown);
    if (grown == NULL) {
        return 0;
    }
    rules->items = grown;
    grown[rules->count].kind = kind;
    grown[rules->count].reason = duplicate(reason, strlen(reason));
    grown[rules->count].pattern = duplicate(pattern, strlen(pattern));
    rules->count++;
    return grown[rules->count - 1].reason != NULL && grown[rules->count - 1].pattern != NULL;
}

static int parse_rule_line(char *line, rule_set *rules, const char *path) {
    char *tokens[MAXIMUM_TOKENS];
    size_t count = split_tokens(line, tokens);
    rule_kind kind;
    if (count == 2 && strcmp(tokens[0], MUTEX_MARKER_KIND) == 0) {
        free(rules->mutex_marker);
        rules->mutex_marker = duplicate(tokens[1], strlen(tokens[1]));
        return rules->mutex_marker != NULL;
    }
    if (count != 3 || !parse_rule_kind(tokens[0], &kind)) {
        fprintf(stderr, "%s: cannot read the rule '%s'\n", path, tokens[0]);
        return 0;
    }
    return add_rule(rules, kind, tokens[1], tokens[2]);
}

static size_t count_rules(const rule_set *rules, rule_kind kind) {
    size_t at;
    size_t count = 0;
    for (at = 0; at < rules->count; at++) {
        count += rules->items[at].kind == kind;
    }
    return count;
}

static int load_rules(const char *path, rule_set *rules) {
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
    if (ok && count_rules(rules, RULE_FORBID) == 0) {
        fprintf(stderr, "%s: no forbid rules, so the gate would refuse nothing\n", path);
        return 0;
    }
    if (ok && rules->mutex_marker == NULL) {
        fprintf(stderr, "%s: no %s line, so a lock call cannot be attributed to the mutex object\n", path,
            MUTEX_MARKER_KIND);
        return 0;
    }
    return ok;
}

static void free_rules(rule_set *rules) {
    size_t at;
    for (at = 0; at < rules->count; at++) {
        free(rules->items[at].reason);
        free(rules->items[at].pattern);
    }
    free(rules->items);
    free(rules->mutex_marker);
}

static int add_normalised(text_list *list, symbol_format format, const char *raw) {
    const char *start = raw;
    size_t length;
    char *copy;
    int ok;
    if (format == FORMAT_MSVC && starts_with(raw, "__imp_")) {
        start += strlen("__imp_");
    } else if (format == FORMAT_APPLE && raw[0] == '_') {
        start++;
    }
    length = strlen(start);
    if (format == FORMAT_GNU || format == FORMAT_APPLE) {
        length = strcspn(start, format == FORMAT_GNU ? "@" : "$");
    }
    copy = duplicate(start, length);
    if (copy == NULL) {
        return 0;
    }
    ok = text_list_add(list, copy);
    free(copy);
    return ok;
}

static int capture(const char *const *arguments, size_t count, char **output) {
    char error[512];
    int exit_code;
    if (!tool_run(arguments, count, output, &exit_code, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 0;
    }
    if (exit_code != 0) {
        fprintf(stderr, "%s exited %d reading %s:\n%s\n", arguments[0], exit_code, arguments[count - 1], *output);
        free(*output);
        return 0;
    }
    return 1;
}

static int add_import(shared_symbols *shared, symbol_format format, const char *raw, const char *source) {
    return add_normalised(&shared->imports, format, raw) && text_list_add(&shared->import_sources, source);
}

static int add_unreadable(shared_symbols *shared, const char *what, const char *source) {
    char line[512];
    snprintf(line, sizeof line, "%s from %s, which names no symbol the gate can judge", what, source);
    return text_list_add(&shared->unreadable, line);
}

static int parse_msvc_import_line(char *line, const char **source, shared_symbols *shared) {
    char *tokens[MAXIMUM_TOKENS];
    size_t count = split_tokens(line, tokens);
    if (count == 1 && ends_with_ignoring_case(tokens[0], ".dll")) {
        *source = tokens[0];
        return 1;
    }
    if (count == 2 && is_hex(tokens[0])) {
        return add_import(shared, FORMAT_MSVC, tokens[1], *source);
    }
    if (count == 2 && strcmp(tokens[0], "Ordinal") == 0) {
        return add_unreadable(shared, "an import by ordinal", *source);
    }
    return 1;
}

static int parse_msvc_imports(char *output, shared_symbols *shared) {
    char *cursor = output;
    char *line;
    const char *source = "an unnamed library";
    int in_section = 0;
    int ok = 1;
    while (ok && (line = next_line(&cursor)) != NULL) {
        const char *text = skip_spaces(line);
        if (strstr(text, "delay load imports") != NULL) {
            ok = add_unreadable(shared, "a delay-load import section", "the library");
            in_section = 0;
        } else if (starts_with(text, "Section contains the following imports")) {
            in_section = 1;
        } else if (strcmp(text, "Summary") == 0) {
            in_section = 0;
        } else if (in_section) {
            ok = parse_msvc_import_line(line, &source, shared);
        }
    }
    return ok;
}

static int is_msvc_export_header(char **tokens, size_t count) {
    return count == 4 && strcmp(tokens[0], "ordinal") == 0 && strcmp(tokens[1], "hint") == 0 &&
           strcmp(tokens[2], "RVA") == 0 && strcmp(tokens[3], "name") == 0;
}

static int parse_msvc_exports(char *output, shared_symbols *shared) {
    char *cursor = output;
    char *line;
    int in_section = 0;
    int ok = 1;
    while (ok && (line = next_line(&cursor)) != NULL) {
        char *tokens[MAXIMUM_TOKENS];
        size_t count = split_tokens(line, tokens);
        if (count > MAXIMUM_TOKENS) {
            continue;
        }
        if (is_msvc_export_header(tokens, count)) {
            in_section = 1;
        } else if (count == 1 && strcmp(tokens[0], "Summary") == 0) {
            in_section = 0;
        } else if (in_section && count >= 4 && is_decimal(tokens[0]) && is_hex(tokens[1]) && is_hex(tokens[2])) {
            ok = text_list_add(&shared->exports, tokens[3]);
        } else if (in_section && count >= 3 && is_decimal(tokens[0]) && is_hex(tokens[1])) {
            ok = text_list_add(&shared->exports, tokens[2]);
        }
    }
    return ok;
}

static int is_undefined_type(char type) {
    return type == 'U' || type == 'w' || type == 'v';
}

static int parse_nm_imports(char *output, symbol_format format, shared_symbols *shared) {
    char *cursor = output;
    char *line;
    int ok = 1;
    while (ok && (line = next_line(&cursor)) != NULL) {
        char *tokens[MAXIMUM_TOKENS];
        size_t count;
        if (line[0] == '\0' || line[strlen(line) - 1] == ':') {
            continue;
        }
        count = split_tokens(line, tokens);
        if (count == 1 && format == FORMAT_APPLE) {
            ok = add_import(shared, format, tokens[0], "the dynamic symbol table");
        } else if (count == 2 && strlen(tokens[0]) == 1 && is_undefined_type(tokens[0][0])) {
            ok = add_import(shared, format, tokens[1], "the dynamic symbol table");
        }
    }
    return ok;
}

static int parse_nm_exports(char *output, symbol_format format, shared_symbols *shared) {
    char *cursor = output;
    char *line;
    int ok = 1;
    while (ok && (line = next_line(&cursor)) != NULL) {
        char *tokens[MAXIMUM_TOKENS];
        size_t count = split_tokens(line, tokens);
        if (count == 3 && is_hex(tokens[0]) && strlen(tokens[1]) == 1 && isupper((unsigned char)tokens[1][0]) &&
            tokens[1][0] != 'U') {
            ok = add_normalised(&shared->exports, format, tokens[2]);
        }
    }
    return ok;
}

static int read_shared(symbol_format format, const char *tool, const char *path, shared_symbols *shared) {
    const char *msvc_imports[] = {tool, "/nologo", "/imports", path};
    const char *msvc_exports[] = {tool, "/nologo", "/exports", path};
    const char *gnu_imports[] = {tool, "-D", "--undefined-only", path};
    const char *gnu_exports[] = {tool, "-D", "--defined-only", path};
    const char *apple_imports[] = {tool, "-u", path};
    const char *apple_exports[] = {tool, "-g", "-U", path};
    char *output;
    int ok;
    if (format == FORMAT_MSVC) {
        if (!capture(msvc_imports, 4, &output)) {
            return 0;
        }
        ok = parse_msvc_imports(output, shared);
        free(output);
        if (!ok || !capture(msvc_exports, 4, &output)) {
            return 0;
        }
        ok = parse_msvc_exports(output, shared);
    } else {
        if (!(format == FORMAT_GNU ? capture(gnu_imports, 4, &output) : capture(apple_imports, 3, &output))) {
            return 0;
        }
        ok = parse_nm_imports(output, format, shared);
        free(output);
        if (!ok || !(format == FORMAT_GNU ? capture(gnu_exports, 4, &output) : capture(apple_exports, 4, &output))) {
            return 0;
        }
        ok = parse_nm_exports(output, format, shared);
    }
    free(output);
    return ok;
}

static object_symbols *start_object(object_list *objects, const char *label) {
    object_symbols *grown = realloc(objects->items, (objects->count + 1) * sizeof *grown);
    if (grown == NULL) {
        return NULL;
    }
    objects->items = grown;
    memset(&grown[objects->count], 0, sizeof grown[objects->count]);
    grown[objects->count].label = duplicate(label, strlen(label));
    if (grown[objects->count].label == NULL) {
        return NULL;
    }
    return &grown[objects->count++];
}

static int record_msvc_symbol(char *line, object_symbols *object) {
    char *bar = strstr(line, "| ");
    char *left_tokens[MAXIMUM_TOKENS];
    char *right_tokens[MAXIMUM_TOKENS];
    size_t left_count;
    const char *section;
    const char *storage;
    *bar = '\0';
    left_count = split_tokens(line, left_tokens);
    if (left_count < 4 || left_count > MAXIMUM_TOKENS || split_tokens(bar + 2, right_tokens) == 0) {
        return 1;
    }
    section = left_tokens[2];
    storage = left_tokens[left_count - 1];
    if (!add_normalised(&object->named, FORMAT_MSVC, right_tokens[0])) {
        return 0;
    }
    if (strcmp(storage, "External") != 0) {
        return 1;
    }
    if (strcmp(section, "UNDEF") == 0) {
        return add_normalised(&object->undefined, FORMAT_MSVC, right_tokens[0]);
    }
    if (starts_with(section, "SECT")) {
        return add_normalised(&object->defined, FORMAT_MSVC, right_tokens[0]);
    }
    return 1;
}

static char *archive_member_label(char *line) {
    char *tokens[MAXIMUM_TOKENS];
    size_t count = split_tokens(line, tokens);
    const char *last = tokens[(count > MAXIMUM_TOKENS ? MAXIMUM_TOKENS : count) - 1];
    return duplicate(base_name(last), strlen(base_name(last)));
}

static int parse_msvc_objects(char *output, const char *path, object_list *objects) {
    char *cursor = output;
    char *line;
    char *member_label = NULL;
    object_symbols *object = NULL;
    int ok = 1;
    while (ok && (line = next_line(&cursor)) != NULL) {
        const char *text = skip_spaces(line);
        if (starts_with(text, "Archive member name at ")) {
            free(member_label);
            member_label = archive_member_label(line);
            ok = member_label != NULL;
        } else if (strcmp(text, "COFF SYMBOL TABLE") == 0) {
            object = start_object(objects, member_label != NULL ? member_label : base_name(path));
            ok = object != NULL;
            free(member_label);
            member_label = NULL;
        } else if (object != NULL && strstr(line, "| ") != NULL) {
            ok = record_msvc_symbol(line, object);
        }
    }
    free(member_label);
    return ok;
}

static int is_nm_member_header(const char *line) {
    size_t length = strlen(line);
    return length > 1 && line[0] != ' ' && line[0] != '\t' && line[length - 1] == ':';
}

static int record_nm_symbol(object_symbols *object, symbol_format format, int has_address, char type,
    const char *name) {
    if (!add_normalised(&object->named, format, name)) {
        return 0;
    }
    if (!has_address && is_undefined_type(type)) {
        return add_normalised(&object->undefined, format, name);
    }
    if (has_address && isupper((unsigned char)type) && type != 'U') {
        return add_normalised(&object->defined, format, name);
    }
    return 1;
}

static int parse_nm_objects(char *output, symbol_format format, const char *path, object_list *objects) {
    char *cursor = output;
    char *line;
    object_symbols *object = NULL;
    int ok = 1;
    while (ok && (line = next_line(&cursor)) != NULL) {
        char *tokens[MAXIMUM_TOKENS];
        size_t count;
        int has_address;
        if (is_nm_member_header(line)) {
            line[strlen(line) - 1] = '\0';
            object = start_object(objects, base_name(line));
            ok = object != NULL;
            continue;
        }
        count = split_tokens(line, tokens);
        has_address = count == 3 && is_hex(tokens[0]) && strlen(tokens[1]) == 1;
        if (!has_address && !(count == 2 && strlen(tokens[0]) == 1)) {
            continue;
        }
        if (object == NULL) {
            object = start_object(objects, base_name(path));
            if (object == NULL) {
                return 0;
            }
        }
        ok = has_address ? record_nm_symbol(object, format, 1, tokens[1][0], tokens[2])
                         : record_nm_symbol(object, format, 0, tokens[0][0], tokens[1]);
    }
    return ok;
}

static int read_objects(symbol_format format, const char *tool, const char *path, object_list *objects) {
    const char *msvc_symbols[] = {tool, "/nologo", "/archivemembers", "/symbols", path};
    const char *nm_symbols[] = {tool, path};
    char *output;
    int ok;
    if (!(format == FORMAT_MSVC ? capture(msvc_symbols, 5, &output) : capture(nm_symbols, 2, &output))) {
        return 0;
    }
    ok = format == FORMAT_MSVC ? parse_msvc_objects(output, path, objects)
                               : parse_nm_objects(output, format, path, objects);
    free(output);
    return ok;
}

static int defined_in_the_objects(const object_list *objects, const char *symbol) {
    size_t at;
    for (at = 0; at < objects->count; at++) {
        if (list_contains(&objects->items[at].defined, symbol)) {
            return 1;
        }
    }
    return 0;
}

static const object_symbols *referencing_object(const object_list *objects, const char *symbol) {
    size_t at;
    for (at = 0; at < objects->count; at++) {
        if (list_contains(&objects->items[at].undefined, symbol)) {
            return &objects->items[at];
        }
    }
    return NULL;
}

static int is_allowed_outside_the_mutex(const rule_set *rules, const char *symbol) {
    return find_rule(rules, RULE_ALLOW_EVERYWHERE, symbol) != NULL ||
           find_rule(rules, RULE_ALLOW_FROM_COMPILER, symbol) != NULL;
}

static void check_object_reference(verdict *result, const object_symbols *object, const char *symbol) {
    const rule *forbidden = find_rule(result->rules, RULE_FORBID, symbol);
    if (find_rule(result->rules, RULE_FORBID_NAME, symbol) != NULL) {
        return;
    }
    if (forbidden != NULL) {
        fprintf(stderr, "%s: %s reads the world (%s)\n", object->label, symbol, forbidden->reason);
    } else if (defined_in_the_objects(result->objects, symbol) ||
               is_allowed_outside_the_mutex(result->rules, symbol)) {
        return;
    } else if (find_rule(result->rules, RULE_ALLOW_IN_MUTEX, symbol) != NULL) {
        if (object == result->mutex_object) {
            return;
        }
        fprintf(stderr, "%s: %s is a lock call outside the mutex object, %s\n", object->label, symbol,
            result->mutex_object->label);
    } else {
        fprintf(stderr, "%s: %s is not on the allow list\n", object->label, symbol);
    }
    result->offenders++;
}

static void check_names(verdict *result, const char *where, const text_list *names) {
    size_t at;
    for (at = 0; at < names->count; at++) {
        const rule *forbidden = find_rule(result->rules, RULE_FORBID_NAME, names->items[at]);
        if (forbidden != NULL) {
            fprintf(stderr, "%s: %s carries a forbidden name (%s)\n", where, names->items[at], forbidden->reason);
            result->offenders++;
        }
    }
}

static void check_objects(verdict *result) {
    size_t object_at;
    size_t at;
    for (object_at = 0; object_at < result->objects->count; object_at++) {
        const object_symbols *object = &result->objects->items[object_at];
        for (at = 0; at < object->undefined.count; at++) {
            check_object_reference(result, object, object->undefined.items[at]);
        }
        check_names(result, object->label, &object->named);
    }
}

static void check_startup_import(verdict *result, const char *shared_path, const char *symbol, const char *source,
    const rule *forbidden, const rule *startup) {
    const object_symbols *caller;
    if (result->objects->count == 0) {
        fprintf(stderr,
            "%s: %s, imported from %s, reads the world (%s); the %s startup code draws it in, but without the core's "
            "objects nothing proves the core does not call it\n",
            shared_path, symbol, source, forbidden->reason, startup->reason);
        result->offenders++;
        return;
    }
    caller = referencing_object(result->objects, symbol);
    if (caller != NULL) {
        fprintf(stderr, "%s: %s, imported from %s, reads the world (%s) and %s calls it\n", shared_path, symbol,
            source, forbidden->reason, caller->label);
        result->offenders++;
        return;
    }
    printf("%s: %s, imported from %s, reads the world (%s), but only the %s startup code calls it: no core object "
           "references it\n",
        shared_path, symbol, source, forbidden->reason, startup->reason);
}

static int check_import(verdict *result, const char *shared_path, const char *symbol, const char *source) {
    const rule *forbidden = find_rule(result->rules, RULE_FORBID, symbol);
    const rule *startup = find_rule(result->rules, RULE_ALLOW_FROM_STARTUP, symbol);
    if (find_rule(result->rules, RULE_FORBID_NAME, symbol) != NULL) {
        return 0;
    }
    if (forbidden != NULL && startup != NULL) {
        check_startup_import(result, shared_path, symbol, source, forbidden, startup);
    } else if (forbidden != NULL) {
        fprintf(stderr, "%s: %s, imported from %s, reads the world (%s)\n", shared_path, symbol, source,
            forbidden->reason);
        result->offenders++;
    } else if (find_rule(result->rules, RULE_ALLOW_IN_MUTEX, symbol) != NULL) {
        return 1;
    } else if (startup == NULL && !is_allowed_outside_the_mutex(result->rules, symbol)) {
        fprintf(stderr, "%s: %s, imported from %s, is not on the allow list\n", shared_path, symbol, source);
        result->offenders++;
    }
    return 0;
}

static void report_lock_attribution(const verdict *result, const char *shared_path, const text_list *locks) {
    size_t at;
    if (locks->count == 0) {
        return;
    }
    printf("lock calls");
    for (at = 0; at < locks->count; at++) {
        printf(" %s", locks->items[at]);
    }
    if (result->mutex_object != NULL) {
        printf(": the import table of %s cannot say which object makes them, so they were attributed through the "
               "objects instead, where only %s may\n",
            shared_path, result->mutex_object->label);
    } else {
        printf(": the import table of %s cannot say which object makes them, and no objects were given, so they are "
               "NOT attributed to the mutex object\n",
            shared_path);
    }
}

static int check_shared(verdict *result, const char *shared_path, const shared_symbols *shared) {
    text_list locks = {NULL, 0};
    size_t at;
    int ok = 1;
    for (at = 0; at < shared->unreadable.count; at++) {
        fprintf(stderr, "%s: %s\n", shared_path, shared->unreadable.items[at]);
        result->offenders++;
    }
    for (at = 0; ok && at < shared->imports.count; at++) {
        const char *symbol = shared->imports.items[at];
        if (check_import(result, shared_path, symbol, shared->import_sources.items[at])) {
            ok = text_list_add(&locks, symbol);
        }
    }
    check_names(result, shared_path, &shared->imports);
    check_names(result, shared_path, &shared->exports);
    report_lock_attribution(result, shared_path, &locks);
    text_list_free(&locks);
    return ok;
}

static size_t count_undefined(const object_list *objects) {
    size_t at;
    size_t count = 0;
    for (at = 0; at < objects->count; at++) {
        count += objects->items[at].undefined.count;
    }
    return count;
}

static int find_mutex_object(const rule_set *rules, const object_list *objects, verdict *result) {
    size_t at;
    size_t found = 0;
    for (at = 0; at < objects->count; at++) {
        if (list_contains(&objects->items[at].defined, rules->mutex_marker)) {
            result->mutex_object = &objects->items[at];
            found++;
        }
    }
    if (found != 1) {
        fprintf(stderr, "%zu objects define %s, so the mutex object is not known\n", found, rules->mutex_marker);
        return 0;
    }
    return 1;
}

static int parse_format(const char *name, symbol_format *format) {
    if (strcmp(name, "msvc") == 0) {
        *format = FORMAT_MSVC;
    } else if (strcmp(name, "gnu") == 0) {
        *format = FORMAT_GNU;
    } else if (strcmp(name, "apple") == 0) {
        *format = FORMAT_APPLE;
    } else {
        return 0;
    }
    return 1;
}

static int read_everything(symbol_format format, const char *tool, const char *shared_path,
    const char *const *object_paths, size_t object_path_count, shared_symbols *shared, object_list *objects) {
    size_t at;
    if (!read_shared(format, tool, shared_path, shared)) {
        return 0;
    }
    if (shared->imports.count == 0 && shared->unreadable.count == 0) {
        fprintf(stderr, "no imports read from %s, so nothing was checked\n", shared_path);
        return 0;
    }
    if (shared->exports.count == 0) {
        fprintf(stderr, "no exports read from %s, so nothing was checked\n", shared_path);
        return 0;
    }
    for (at = 0; at < object_path_count; at++) {
        if (!read_objects(format, tool, object_paths[at], objects)) {
            return 0;
        }
    }
    if (object_path_count > 0 && (objects->count == 0 || count_undefined(objects) == 0)) {
        fprintf(stderr, "no object references read from the objects given, so nothing was checked\n");
        return 0;
    }
    return 1;
}

static void free_objects(object_list *objects) {
    size_t at;
    for (at = 0; at < objects->count; at++) {
        free(objects->items[at].label);
        text_list_free(&objects->items[at].defined);
        text_list_free(&objects->items[at].undefined);
        text_list_free(&objects->items[at].named);
    }
    free(objects->items);
}

static void free_shared(shared_symbols *shared) {
    text_list_free(&shared->imports);
    text_list_free(&shared->import_sources);
    text_list_free(&shared->exports);
    text_list_free(&shared->unreadable);
}

static int judge(const rule_set *rules, const char *shared_path, const shared_symbols *shared,
    const object_list *objects) {
    verdict result = {NULL, NULL, NULL, 0};
    result.rules = rules;
    result.objects = objects;
    if (objects->count > 0) {
        if (!find_mutex_object(rules, objects, &result)) {
            return 2;
        }
        check_objects(&result);
    }
    if (!check_shared(&result, shared_path, shared)) {
        fprintf(stderr, "out of memory\n");
        return 2;
    }
    printf("%zu imports and %zu exports of %s read, and %zu objects with %zu references\n", shared->imports.count,
        shared->exports.count, shared_path, objects->count, count_undefined(objects));
    if (result.offenders > 0) {
        fprintf(stderr, "the core reads the world: %zu offenders\n", result.offenders);
        return 1;
    }
    printf("the core reads nothing of the world\n");
    return 0;
}

int main(int argc, char **argv) {
    rule_set rules = {NULL, 0, NULL};
    shared_symbols shared;
    object_list objects = {NULL, 0};
    symbol_format format;
    int exit_code = 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 5 || !parse_format(argv[2], &format)) {
        fprintf(stderr,
            "usage: weivel-core-gate <rules-file> <msvc|gnu|apple> <tool> <shared-library> [<static-library>...]\n");
        return 2;
    }
    memset(&shared, 0, sizeof shared);
    if (load_rules(argv[1], &rules) &&
        read_everything(format, argv[3], argv[4], (const char *const *)argv + 5, (size_t)(argc - 5), &shared,
            &objects)) {
        exit_code = judge(&rules, argv[4], &shared, &objects);
    }
    free_objects(&objects);
    free_shared(&shared);
    free_rules(&rules);
    return exit_code;
}
