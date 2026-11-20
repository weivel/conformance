#include "run.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUN_FILE_NAME "stub-host.json"
#define RECORD_FILE_NAME "record.txt"

typedef struct reading {
    stub_run *run;
    char *detail;
    size_t detail_size;
    int out_of_memory;
} reading;

static int refuse(reading *state, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(state->detail, state->detail_size, format, arguments);
    va_end(arguments);
    return 0;
}

static int out_of_memory(reading *state) {
    state->out_of_memory = 1;
    return refuse(state, "out of memory reading " RUN_FILE_NAME);
}

static char *read_whole_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    long size;
    char *text;
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    text = malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    text[size] = '\0';
    *length = (size_t)size;
    return text;
}

static int is_absolute(const char *path) {
    int drive = ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
    return path[0] == '/' || path[0] == '\\' || drive;
}

static char *join(const char *directory, const char *path) {
    size_t directory_length = strlen(directory);
    size_t path_length = strlen(path);
    char *joined;
    if (is_absolute(path)) {
        return stub_copy_text(path);
    }
    joined = malloc(directory_length + 1 + path_length + 1);
    if (joined != NULL) {
        memcpy(joined, directory, directory_length);
        joined[directory_length] = '/';
        memcpy(joined + directory_length + 1, path, path_length + 1);
    }
    return joined;
}

static const char *text_member(const weivel_json_value *object, const char *key) {
    return weivel_json_as_string(weivel_json_object_get(object, key));
}

static int is_present(const weivel_json_value *object, const char *key) {
    return weivel_json_object_get(object, key) != NULL;
}

/** Reads the text `key` of `object`, joined to the run's directory; 0 with the state refused when it is not one. */
static int read_path(reading *state, const weivel_json_value *object, const char *key, const char *where,
                     char **path) {
    const char *text = text_member(object, key);
    if (text == NULL || text[0] == '\0') {
        return refuse(state, "%s.%s must be a non-empty text", where, key);
    }
    *path = join(state->run->directory, text);
    return *path != NULL || out_of_memory(state);
}

static int read_mode(reading *state, const weivel_json_value *root) {
    const char *mode = text_member(root, "mode");
    if (mode != NULL && strcmp(mode, "serve") == 0) {
        state->run->mode = WEIVEL_STANDARD_HOST_SERVE;
    } else if (mode != NULL && strcmp(mode, "action") == 0) {
        state->run->mode = WEIVEL_STANDARD_HOST_ACTION;
    } else {
        return refuse(state, "mode must be \"serve\" or \"action\"");
    }
    return 1;
}

static int read_libraries(reading *state, const weivel_json_value *root) {
    const weivel_json_value *libraries = weivel_json_object_get(root, "libraries");
    stub_run *run = state->run;
    char where[64];
    size_t at;
    if (libraries == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(libraries) != WEIVEL_JSON_ARRAY) {
        return refuse(state, "libraries must be a list");
    }
    run->library_count = weivel_json_array_length(libraries);
    run->libraries = run->library_count == 0 ? NULL : calloc(run->library_count, sizeof *run->libraries);
    if (run->library_count != 0 && run->libraries == NULL) {
        run->library_count = 0;
        return out_of_memory(state);
    }
    for (at = 0; at < run->library_count; at++) {
        const weivel_json_value *library = weivel_json_array_at(libraries, at);
        char *binary = NULL;
        char *declaration = NULL;
        snprintf(where, sizeof where, "libraries[%lu]", (unsigned long)at);
        if (!read_path(state, library, "binary", where, &binary)) {
            return 0;
        }
        run->libraries[at].binary = binary;
        if (!read_path(state, library, "declaration", where, &declaration)) {
            return 0;
        }
        run->libraries[at].declaration = declaration;
    }
    return 1;
}

static int read_setting(reading *state, const char *key, const weivel_json_value *json, stub_setting *setting,
                        const char *where) {
    int boolean;
    setting->key = stub_copy_text(key);
    if (setting->key == NULL) {
        return out_of_memory(state);
    }
    switch (weivel_json_kind_of(json)) {
    case WEIVEL_JSON_STRING:
        setting->value.kind = WEIVEL_STANDARD_SETTING_TEXT;
        setting->value.text_length = weivel_json_string_length(json);
        if (memchr(weivel_json_as_string(json), '\0', setting->value.text_length) != NULL) {
            return refuse(state, "%s.settings.%s must hold no NUL", where, key);
        }
        setting->value.text = stub_copy_text(weivel_json_as_string(json));
        return setting->value.text != NULL || out_of_memory(state);
    case WEIVEL_JSON_NUMBER:
        setting->value.kind = WEIVEL_STANDARD_SETTING_NUMBER;
        weivel_json_as_double(json, &setting->value.number);
        return 1;
    case WEIVEL_JSON_BOOL:
        setting->value.kind = WEIVEL_STANDARD_SETTING_BOOLEAN;
        weivel_json_as_bool(json, &boolean);
        setting->value.boolean = boolean ? 1u : 0u;
        return 1;
    default:
        return refuse(state, "%s.settings.%s must be a text, a number or a boolean", where, key);
    }
}

static int read_settings(reading *state, const weivel_json_value *entry, stub_plugin *plugin, const char *where) {
    const weivel_json_value *settings = weivel_json_object_get(entry, "settings");
    size_t at;
    if (settings == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(settings) != WEIVEL_JSON_OBJECT) {
        return refuse(state, "%s.settings must be an object", where);
    }
    plugin->setting_count = weivel_json_object_size(settings);
    plugin->settings = plugin->setting_count == 0 ? NULL : calloc(plugin->setting_count, sizeof *plugin->settings);
    if (plugin->setting_count != 0 && plugin->settings == NULL) {
        plugin->setting_count = 0;
        return out_of_memory(state);
    }
    for (at = 0; at < plugin->setting_count; at++) {
        if (!read_setting(state, weivel_json_object_key_at(settings, at), weivel_json_object_value_at(settings, at),
                          &plugin->settings[at], where)) {
            return 0;
        }
    }
    return 1;
}

static int read_scope(reading *state, const weivel_json_value *entry, stub_plugin *plugin, const char *where) {
    const weivel_json_value *scope = weivel_json_object_get(entry, "scope");
    int scoped = 0;
    if (scope != NULL && !weivel_json_as_bool(scope, &scoped)) {
        return refuse(state, "%s.scope must be a boolean", where);
    }
    plugin->scoped = scoped;
    if (is_present(entry, "scopeRefused")) {
        const char *detail = text_member(entry, "scopeRefused");
        if (detail == NULL || detail[0] == '\0') {
            return refuse(state, "%s.scopeRefused must be a non-empty text", where);
        }
        if (scoped) {
            return refuse(state, "%s names both scope and scopeRefused", where);
        }
        plugin->scope_refusal = stub_copy_text(detail);
        if (plugin->scope_refusal == NULL) {
            return out_of_memory(state);
        }
    }
    return 1;
}

static int read_plugin_paths(reading *state, const weivel_json_value *entry, stub_plugin *plugin, const char *where) {
    static const char CACHE[] = "/cache";
    static const char CONFIG[] = "/config";
    size_t directory_length;
    if (!read_path(state, entry, "directory", where, &plugin->directory) ||
        !read_path(state, entry, "manifest", where, &plugin->manifest) ||
        !read_path(state, entry, "entry", where, &plugin->entry)) {
        return 0;
    }
    directory_length = strlen(plugin->directory);
    plugin->cache_directory = malloc(directory_length + sizeof CACHE);
    plugin->config_directory = malloc(directory_length + sizeof CONFIG);
    if (plugin->cache_directory == NULL || plugin->config_directory == NULL) {
        return out_of_memory(state);
    }
    snprintf(plugin->cache_directory, directory_length + sizeof CACHE, "%s%s", plugin->directory, CACHE);
    snprintf(plugin->config_directory, directory_length + sizeof CONFIG, "%s%s", plugin->directory, CONFIG);
    return 1;
}

static int is_plugin_name(const char *name) {
    size_t at;
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (at = 0; name[at] != '\0'; at++) {
        char letter = name[at];
        if (!((letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '-')) {
            return 0;
        }
    }
    return 1;
}

static int read_plugin(reading *state, const weivel_json_value *entry, size_t index) {
    stub_plugin *plugin = &state->run->plugins[index];
    const char *name = text_member(entry, "name");
    char where[64];
    size_t earlier;
    snprintf(where, sizeof where, "plugins[%lu]", (unsigned long)index);
    if (weivel_json_kind_of(entry) != WEIVEL_JSON_OBJECT) {
        return refuse(state, "%s must be an object", where);
    }
    if (!is_plugin_name(name)) {
        return refuse(state, "%s.name must be lowercase letters, digits and hyphens", where);
    }
    for (earlier = 0; earlier < index; earlier++) {
        if (strcmp(state->run->plugins[earlier].name, name) == 0) {
            return refuse(state, "%s.name %s is given twice", where, name);
        }
    }
    plugin->name = stub_copy_text(name);
    if (plugin->name == NULL) {
        return out_of_memory(state);
    }
    return read_plugin_paths(state, entry, plugin, where) && read_scope(state, entry, plugin, where) &&
           read_settings(state, entry, plugin, where);
}

static int read_plugins(reading *state, const weivel_json_value *root) {
    const weivel_json_value *plugins = weivel_json_object_get(root, "plugins");
    stub_run *run = state->run;
    size_t at;
    if (weivel_json_kind_of(plugins) != WEIVEL_JSON_ARRAY) {
        return refuse(state, "plugins must be a list");
    }
    run->plugin_count = weivel_json_array_length(plugins);
    run->plugins = run->plugin_count == 0 ? NULL : calloc(run->plugin_count, sizeof *run->plugins);
    if (run->plugin_count != 0 && run->plugins == NULL) {
        run->plugin_count = 0;
        return out_of_memory(state);
    }
    for (at = 0; at < run->plugin_count; at++) {
        if (!read_plugin(state, weivel_json_array_at(plugins, at), at)) {
            return 0;
        }
    }
    return 1;
}

static const stub_plugin *plugin_by_name(const stub_run *run, const char *name) {
    size_t at;
    for (at = 0; name != NULL && at < run->plugin_count; at++) {
        if (strcmp(run->plugins[at].name, name) == 0) {
            return &run->plugins[at];
        }
    }
    return NULL;
}

static int read_action(reading *state, const weivel_json_value *root) {
    const weivel_json_value *action = weivel_json_object_get(root, "action");
    const weivel_json_value *arguments;
    const char *name;
    stub_run *run = state->run;
    if (run->mode == WEIVEL_STANDARD_HOST_SERVE) {
        return action == NULL || refuse(state, "action is given in serve mode");
    }
    if (weivel_json_kind_of(action) != WEIVEL_JSON_OBJECT) {
        return refuse(state, "action must be an object in action mode");
    }
    run->action_plugin = plugin_by_name(run, text_member(action, "plugin"));
    if (run->action_plugin == NULL) {
        return refuse(state, "action.plugin must name a plugin of plugins");
    }
    name = text_member(action, "name");
    if (name == NULL || name[0] == '\0') {
        return refuse(state, "action.name must be a non-empty text");
    }
    run->action_name = stub_copy_text(name);
    if (run->action_name == NULL) {
        return out_of_memory(state);
    }
    arguments = weivel_json_object_get(action, "arguments");
    if (arguments != NULL && !stub_value_from_json(arguments, &run->action_arguments)) {
        return refuse(state, "action.arguments could not be copied: a text holds NUL, or memory ran out");
    }
    return 1;
}

static int read_ticks(reading *state, const weivel_json_value *root) {
    const weivel_json_value *ticks = weivel_json_object_get(root, "ticks");
    stub_run *run = state->run;
    size_t at;
    if (ticks == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(ticks) != WEIVEL_JSON_ARRAY) {
        return refuse(state, "ticks must be a list");
    }
    run->tick_count = weivel_json_array_length(ticks);
    run->ticks = run->tick_count == 0 ? NULL : calloc(run->tick_count, sizeof *run->ticks);
    if (run->tick_count != 0 && run->ticks == NULL) {
        run->tick_count = 0;
        return out_of_memory(state);
    }
    for (at = 0; at < run->tick_count; at++) {
        if (!weivel_json_as_int64(weivel_json_array_at(ticks, at), &run->ticks[at]) || run->ticks[at] < 0) {
            return refuse(state, "ticks[%lu] must be a whole number of milliseconds, 0 or more", (unsigned long)at);
        }
    }
    return 1;
}

static int read_exit(reading *state, const weivel_json_value *root) {
    const weivel_json_value *exit_code = weivel_json_object_get(root, "exit");
    int64_t code = 0;
    if (exit_code != NULL && (!weivel_json_as_int64(exit_code, &code) || code < 0 || code > 255)) {
        return refuse(state, "exit must be a whole number from 0 to 255");
    }
    state->run->exit_code = (uint32_t)code;
    return 1;
}

static int read_refusal(reading *state, const weivel_json_value *refuse_member, const char *key, char **detail) {
    const weivel_json_value *member = weivel_json_object_get(refuse_member, key);
    const char *text = weivel_json_as_string(member);
    if (member == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0') {
        return refuse(state, "refuse.%s must be a non-empty text", key);
    }
    *detail = stub_copy_text(text);
    return *detail != NULL || out_of_memory(state);
}

static int read_refusals(reading *state, const weivel_json_value *root) {
    const weivel_json_value *refusals = weivel_json_object_get(root, "refuse");
    if (refusals == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(refusals) != WEIVEL_JSON_OBJECT) {
        return refuse(state, "refuse must be an object");
    }
    return read_refusal(state, refusals, "start", &state->run->start_refusal) &&
           read_refusal(state, refusals, "started", &state->run->started_refusal);
}

static int read_root(reading *state, const weivel_json_value *root) {
    if (weivel_json_kind_of(root) != WEIVEL_JSON_OBJECT) {
        return refuse(state, RUN_FILE_NAME " must hold an object");
    }
    return read_mode(state, root) && read_libraries(state, root) && read_plugins(state, root) &&
           read_action(state, root) && read_ticks(state, root) && read_exit(state, root) &&
           read_refusals(state, root);
}

static int read_text(reading *state, const char *text, size_t length) {
    weivel_json_error error = WEIVEL_INIT(weivel_json_error);
    weivel_json_value *root = weivel_json_parse(text, length, &error);
    int read;
    if (root == NULL) {
        if (error.out_of_memory) {
            return out_of_memory(state);
        }
        return refuse(state, RUN_FILE_NAME " is not JSON at line %d, column %d: %s", error.line, error.column,
                      error.message == NULL ? "malformed" : error.message);
    }
    read = read_root(state, root);
    weivel_json_free(root);
    return read;
}

static int begin_run(reading *state, const char *directory) {
    state->run = calloc(1, sizeof *state->run);
    if (state->run == NULL) {
        return out_of_memory(state);
    }
    state->run->directory = stub_copy_text(directory);
    if (state->run->directory == NULL) {
        return out_of_memory(state);
    }
    state->run->record_path = join(directory, RECORD_FILE_NAME);
    return state->run->record_path != NULL || out_of_memory(state);
}

weivel_status stub_run_read(const char *directory, stub_run **run, char *detail, size_t detail_size) {
    reading state = {NULL, NULL, 0, 0};
    char *path;
    char *text = NULL;
    size_t length = 0;
    int read;
    state.detail = detail;
    state.detail_size = detail_size;
    *run = NULL;
    read = begin_run(&state, directory);
    path = read ? join(directory, RUN_FILE_NAME) : NULL;
    if (read && path == NULL) {
        read = out_of_memory(&state);
    }
    if (read) {
        text = read_whole_file(path, &length);
        read = text != NULL ? read_text(&state, text, length) : refuse(&state, "cannot read %s", path);
    }
    free(text);
    free(path);
    if (!read) {
        stub_run_free(state.run);
        return state.out_of_memory ? WEIVEL_STATUS_OUT_OF_MEMORY : WEIVEL_STATUS_REFUSED;
    }
    *run = state.run;
    return WEIVEL_STATUS_OK;
}

static void free_plugin(stub_plugin *plugin) {
    size_t at;
    free(plugin->name);
    free(plugin->directory);
    free(plugin->manifest);
    free(plugin->entry);
    free(plugin->cache_directory);
    free(plugin->config_directory);
    free(plugin->scope_refusal);
    for (at = 0; at < plugin->setting_count; at++) {
        free(plugin->settings[at].key);
        free((void *)plugin->settings[at].value.text);
    }
    free(plugin->settings);
    for (at = 0; at < plugin->secret_count; at++) {
        free(plugin->secrets[at].name);
        free(plugin->secrets[at].bytes);
    }
    free(plugin->secrets);
}

void stub_run_free(stub_run *run) {
    size_t at;
    if (run == NULL) {
        return;
    }
    for (at = 0; at < run->library_count; at++) {
        free((void *)run->libraries[at].binary);
        free((void *)run->libraries[at].declaration);
    }
    free(run->libraries);
    for (at = 0; at < run->plugin_count; at++) {
        free_plugin(&run->plugins[at]);
    }
    free(run->plugins);
    free(run->action_name);
    stub_value_free(&run->action_arguments);
    free(run->ticks);
    free(run->start_refusal);
    free(run->started_refusal);
    free(run->directory);
    free(run->record_path);
    free(run);
}

stub_plugin *stub_run_plugin_by_manifest(const stub_run *run, const char *manifest) {
    size_t at;
    for (at = 0; manifest != NULL && at < run->plugin_count; at++) {
        if (strcmp(run->plugins[at].manifest, manifest) == 0) {
            return &run->plugins[at];
        }
    }
    return NULL;
}
