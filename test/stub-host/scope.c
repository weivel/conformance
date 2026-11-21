#include "run.h"
#include "stub_host.h"

#include "weivel/descriptions.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SCOPE_FIRST_SHAPE (offsetof(weivel_standard_scope, storage) + sizeof(void *))
#define SETTINGS_FIRST_SHAPE (offsetof(weivel_standard_settings, storage) + sizeof(void *))
#define ANSWER_FIRST_SHAPE (offsetof(weivel_standard_setting_answer, storage) + sizeof(void *))
#define SECRET_FIRST_SHAPE (offsetof(weivel_standard_secret, storage) + sizeof(void *))

static const char *const LEVEL_NAMES[] = {"trace", "debug", "info", "warn", "error"};

/** Appends `line` to the run's record and frees it; the caller holds the lock. */
static void record_and_free(stub_line *line) {
    stub_run *run = stub_host_current_run();
    if (run != NULL && line->text != NULL) {
        stub_record_line(run->record_path, line->text);
    }
    stub_line_free(line);
}

static stub_setting *find_setting(const stub_plugin *plugin, const char *key) {
    size_t at;
    for (at = 0; at < plugin->setting_count; at++) {
        if (strcmp(plugin->settings[at].key, key) == 0) {
            return &plugin->settings[at];
        }
    }
    return NULL;
}

static int copy_setting_value(const weivel_standard_setting_value *from, weivel_standard_setting_value *to) {
    *to = *from;
    if (from->kind != WEIVEL_STANDARD_SETTING_TEXT) {
        return 1;
    }
    to->text = malloc(from->text_length + 1);
    if (to->text == NULL) {
        return 0;
    }
    memcpy((char *)to->text, from->text, from->text_length + 1);
    return 1;
}

static void free_setting_copies(weivel_standard_setting *copies, size_t count) {
    size_t at;
    for (at = 0; at < count; at++) {
        free((void *)copies[at].key);
        free((void *)copies[at].value.text);
    }
    free(copies);
}

/** One block of `count` settings, each key and text its own allocation, freed by free_setting_copies. */
static weivel_standard_setting *copy_settings(const stub_plugin *plugin, size_t *count) {
    weivel_standard_setting *copies = calloc(plugin->setting_count + 1, sizeof *copies);
    size_t at;
    for (at = 0; copies != NULL && at < plugin->setting_count; at++) {
        copies[at].key = stub_copy_text(plugin->settings[at].key);
        if (copies[at].key == NULL || !copy_setting_value(&plugin->settings[at].value, &copies[at].value)) {
            free_setting_copies(copies, at + 1);
            return NULL;
        }
    }
    *count = plugin->setting_count;
    return copies;
}

static weivel_status config_all(void *context, weivel_standard_settings *settings,
                                weivel_standard_service_refusal *refusal) {
    weivel_standard_setting *copies;
    size_t count = 0;
    if (settings == NULL || !stub_is_empty_answer(settings->size, SETTINGS_FIRST_SHAPE, settings->storage) ||
        !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    copies = copy_settings(context, &count);
    stub_unlock();
    if (copies == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    stub_clear_beyond(settings, settings->size, sizeof *settings);
    settings->settings = copies;
    settings->count = count;
    settings->storage = copies;
    return WEIVEL_STATUS_OK;
}

static weivel_status config_get(void *context, const char *key, weivel_standard_setting_answer *answer,
                                weivel_standard_service_refusal *refusal) {
    const stub_plugin *plugin = context;
    const stub_setting *setting;
    weivel_standard_setting_value copy = {.kind = WEIVEL_STANDARD_SETTING_NONE};
    int copied = 0;
    if (key == NULL || answer == NULL || !stub_is_empty_answer(answer->size, ANSWER_FIRST_SHAPE, answer->storage) ||
        !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    setting = find_setting(plugin, key);
    if (setting != NULL) {
        copied = copy_setting_value(&setting->value, &copy);
    }
    stub_unlock();
    if (setting == NULL) {
        return WEIVEL_STATUS_NONE;
    }
    if (!copied) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    stub_clear_beyond(answer, answer->size, sizeof *answer);
    answer->value = copy;
    answer->storage = copy.kind == WEIVEL_STANDARD_SETTING_TEXT ? (void *)copy.text : (void *)answer;
    return WEIVEL_STATUS_OK;
}

static int is_storable(const weivel_standard_setting_value *value) {
    switch (value->kind) {
    case WEIVEL_STANDARD_SETTING_TEXT:
        return value->text != NULL && memchr(value->text, '\0', value->text_length) == NULL;
    case WEIVEL_STANDARD_SETTING_NUMBER:
        return isfinite(value->number);
    case WEIVEL_STANDARD_SETTING_BOOLEAN:
        return value->boolean == 0 || value->boolean == 1;
    default:
        return 0;
    }
}

static int store_setting(stub_plugin *plugin, const char *key, const weivel_standard_setting_value *value) {
    stub_setting *setting = find_setting(plugin, key);
    weivel_standard_setting_value copy;
    if (!copy_setting_value(value, &copy)) {
        return 0;
    }
    if (setting == NULL) {
        stub_setting *grown = realloc(plugin->settings, (plugin->setting_count + 1) * sizeof *grown);
        char *key_copy = stub_copy_text(key);
        if (grown != NULL) {
            plugin->settings = grown;
        }
        if (grown == NULL || key_copy == NULL) {
            free(key_copy);
            free((void *)copy.text);
            return 0;
        }
        setting = &plugin->settings[plugin->setting_count++];
        setting->key = key_copy;
        setting->value.text = NULL;
    }
    free((void *)setting->value.text);
    setting->value = copy;
    return 1;
}

static weivel_status config_set(void *context, const char *key, const weivel_standard_setting_value *value,
                                weivel_standard_service_refusal *refusal) {
    stub_plugin *plugin = context;
    stub_line line = {NULL, 0, 0};
    int stored;
    if (key == NULL || value == NULL || !is_storable(value) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    stored = store_setting(plugin, key, value);
    if (stored) {
        stub_line_add(&line, "set ");
        stub_line_add(&line, plugin->name);
        stub_line_add(&line, " ");
        stub_line_add_quoted(&line, key, strlen(key));
        stub_line_add(&line, " ");
        stub_line_add_setting(&line, value);
        record_and_free(&line);
    }
    stub_unlock();
    return stored ? WEIVEL_STATUS_OK : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static void config_release_settings(void *context, weivel_standard_settings *settings) {
    (void)context;
    if (settings == NULL || settings->size < SETTINGS_FIRST_SHAPE || settings->storage == NULL) {
        return;
    }
    free_setting_copies(settings->storage, settings->count);
    settings->settings = NULL;
    settings->count = 0;
    settings->storage = NULL;
}

static void config_release_answer(void *context, weivel_standard_setting_answer *answer) {
    (void)context;
    if (answer == NULL || answer->size < ANSWER_FIRST_SHAPE || answer->storage == NULL) {
        return;
    }
    free((void *)answer->value.text);
    memset(&answer->value, 0, sizeof answer->value);
    answer->storage = NULL;
}

static stub_secret *find_secret(const stub_plugin *plugin, const char *name) {
    size_t at;
    for (at = 0; at < plugin->secret_count; at++) {
        if (strcmp(plugin->secrets[at].name, name) == 0) {
            return &plugin->secrets[at];
        }
    }
    return NULL;
}

static weivel_status secrets_remove(void *context, const char *name, weivel_standard_service_refusal *refusal) {
    stub_plugin *plugin = context;
    stub_secret *secret;
    if (name == NULL || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    secret = find_secret(plugin, name);
    if (secret != NULL) {
        free(secret->name);
        free(secret->bytes);
        *secret = plugin->secrets[--plugin->secret_count];
    }
    stub_unlock();
    return WEIVEL_STATUS_OK;
}

static weivel_status secrets_get(void *context, const char *name, weivel_standard_secret *secret,
                                 weivel_standard_service_refusal *refusal) {
    const stub_plugin *plugin = context;
    const stub_secret *kept;
    uint8_t *copy = NULL;
    size_t length = 0;
    if (name == NULL || secret == NULL || !stub_is_empty_answer(secret->size, SECRET_FIRST_SHAPE, secret->storage) ||
        !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    kept = find_secret(plugin, name);
    if (kept != NULL) {
        length = kept->length;
        copy = malloc(length + 1);
        if (copy != NULL && length != 0) {
            memcpy(copy, kept->bytes, length);
        }
    }
    stub_unlock();
    if (kept == NULL) {
        return WEIVEL_STATUS_NONE;
    }
    if (copy == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    stub_clear_beyond(secret, secret->size, sizeof *secret);
    secret->bytes = copy;
    secret->length = length;
    secret->storage = copy;
    return WEIVEL_STATUS_OK;
}

static int store_secret(stub_plugin *plugin, const char *name, const uint8_t *bytes, size_t length) {
    stub_secret *secret = find_secret(plugin, name);
    uint8_t *copy = malloc(length + 1);
    if (copy == NULL) {
        return 0;
    }
    if (length != 0) {
        memcpy(copy, bytes, length);
    }
    if (secret == NULL) {
        stub_secret *grown = realloc(plugin->secrets, (plugin->secret_count + 1) * sizeof *grown);
        char *name_copy = stub_copy_text(name);
        if (grown != NULL) {
            plugin->secrets = grown;
        }
        if (grown == NULL || name_copy == NULL) {
            free(name_copy);
            free(copy);
            return 0;
        }
        secret = &plugin->secrets[plugin->secret_count++];
        secret->name = name_copy;
        secret->bytes = NULL;
    }
    free(secret->bytes);
    secret->bytes = copy;
    secret->length = length;
    return 1;
}

static weivel_status secrets_set(void *context, const char *name, const uint8_t *bytes, size_t length,
                                 weivel_standard_service_refusal *refusal) {
    int stored;
    if (name == NULL || (bytes == NULL && length != 0) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    stored = store_secret(context, name, bytes, length);
    stub_unlock();
    return stored ? WEIVEL_STATUS_OK : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static void secrets_release_secret(void *context, weivel_standard_secret *secret) {
    (void)context;
    if (secret == NULL || secret->size < SECRET_FIRST_SHAPE || secret->storage == NULL) {
        return;
    }
    memset(secret->storage, 0, secret->length);
    free(secret->storage);
    secret->bytes = NULL;
    secret->length = 0;
    secret->storage = NULL;
}

static weivel_status answer_directory(const char *path, const char **directory) {
    if (directory == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *directory = path;
    return WEIVEL_STATUS_OK;
}

static weivel_status paths_cache(void *context, const char **directory) {
    return answer_directory(((const stub_plugin *)context)->cache_directory, directory);
}

static weivel_status paths_config(void *context, const char **directory) {
    return answer_directory(((const stub_plugin *)context)->config_directory, directory);
}

static weivel_status paths_plugin(void *context, const char **directory) {
    return answer_directory(((const stub_plugin *)context)->directory, directory);
}

static weivel_status log_write(void *context, uint32_t level, const char *message, const weivel_standard_value *fields,
                               weivel_standard_service_refusal *refusal) {
    const stub_plugin *plugin = context;
    stub_line line = {NULL, 0, 0};
    if (level < WEIVEL_STANDARD_LOG_TRACE || level > WEIVEL_STANDARD_LOG_ERROR || message == NULL ||
        (fields != NULL && fields->kind != WEIVEL_STANDARD_VALUE_RECORD) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_line_add(&line, "log ");
    stub_line_add(&line, plugin->name);
    stub_line_add(&line, " ");
    stub_line_add(&line, LEVEL_NAMES[level - 1]);
    stub_line_add(&line, " ");
    stub_line_add_quoted(&line, message, strlen(message));
    if (fields != NULL) {
        stub_line_add(&line, " ");
        stub_line_add_value(&line, fields);
    }
    stub_lock();
    record_and_free(&line);
    stub_unlock();
    return WEIVEL_STATUS_OK;
}

void stub_scope_prepare(stub_run *run) {
    size_t at;
    for (at = 0; at < run->plugin_count; at++) {
        stub_plugin *plugin = &run->plugins[at];
        weivel_standard_config config = {
            .size = sizeof config,
            .context = plugin,
            .all = config_all,
            .get = config_get,
            .set = config_set,
            .release_settings = config_release_settings,
            .release_answer = config_release_answer,
            .release_refusal = stub_release_refusal,
        };
        weivel_standard_secrets secrets = {
            .size = sizeof secrets,
            .context = plugin,
            .remove = secrets_remove,
            .get = secrets_get,
            .set = secrets_set,
            .release_secret = secrets_release_secret,
            .release_refusal = stub_release_refusal,
        };
        weivel_standard_paths paths = {
            .size = sizeof paths,
            .context = plugin,
            .cache = paths_cache,
            .config = paths_config,
            .plugin = paths_plugin,
        };
        weivel_standard_log log = {
            .size = sizeof log,
            .context = plugin,
            .write = log_write,
            .release_refusal = stub_release_refusal,
        };
        plugin->config = config;
        plugin->secrets_definition = secrets;
        plugin->paths = paths;
        plugin->log = log;
    }
}

static void record_open(const stub_run *run, const stub_plugin *plugin, const char *answer) {
    stub_line line = {NULL, 0, 0};
    stub_line_add(&line, "open ");
    stub_line_add(&line, plugin == NULL ? "unknown" : plugin->name);
    stub_line_add(&line, " ");
    stub_line_add(&line, answer);
    if (line.text != NULL) {
        stub_record_line(run->record_path, line.text);
    }
    stub_line_free(&line);
}

static weivel_status scopes_open(void *context, const char *manifest_path, weivel_standard_scope *scope,
                                 weivel_standard_service_refusal *refusal) {
    stub_run *run;
    stub_plugin *plugin;
    weivel_status status = WEIVEL_STATUS_OK;
    (void)context;
    if (manifest_path == NULL || scope == NULL ||
        !stub_is_empty_answer(scope->size, SCOPE_FIRST_SHAPE, scope->storage) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    run = stub_host_current_run();
    plugin = run == NULL ? NULL : stub_run_plugin_by_manifest(run, manifest_path);
    if (plugin == NULL) {
        if (run != NULL) {
            record_open(run, NULL, "refused");
        }
        status = stub_refuse(refusal, "the stub host's run names no plugin of that manifest",
                             "open the scope of a manifest the plan named, exactly as it named it");
    } else if (plugin->scope_refusal != NULL) {
        record_open(run, plugin, "refused");
        status = stub_refuse(refusal, plugin->scope_refusal, "remove scopeRefused from the plugin in stub-host.json");
    } else {
        record_open(run, plugin, plugin->scoped ? "scoped" : "empty");
        stub_clear_beyond(scope, scope->size, sizeof *scope);
        scope->config = plugin->scoped ? &plugin->config : NULL;
        scope->secrets = plugin->scoped ? &plugin->secrets_definition : NULL;
        scope->paths = plugin->scoped ? &plugin->paths : NULL;
        scope->log = plugin->scoped ? &plugin->log : NULL;
        scope->storage = plugin;
    }
    stub_unlock();
    return status;
}

static void scopes_release_scope(void *context, weivel_standard_scope *scope) {
    (void)context;
    if (scope == NULL || scope->size < SCOPE_FIRST_SHAPE || scope->storage == NULL) {
        return;
    }
    scope->config = NULL;
    scope->secrets = NULL;
    scope->paths = NULL;
    scope->log = NULL;
    scope->storage = NULL;
}

static const weivel_standard_scopes SCOPES = {
    .size = sizeof(weivel_standard_scopes),
    .context = NULL,
    .open = scopes_open,
    .release_scope = scopes_release_scope,
    .release_refusal = stub_release_refusal,
};

const void *stub_host_scopes_definition(void) {
    return &SCOPES;
}

const weivel_standard_description *stub_host_scopes_description(void) {
    return weivel_standard_scopes_description();
}

static weivel_status library_log_write(void *context, uint32_t level, const char *message,
                                       const weivel_standard_value *fields, weivel_standard_service_refusal *refusal) {
    (void)context;
    if (level < WEIVEL_STANDARD_LOG_TRACE || level > WEIVEL_STANDARD_LOG_ERROR || message == NULL ||
        (fields != NULL && fields->kind != WEIVEL_STANDARD_VALUE_RECORD) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    return WEIVEL_STATUS_OK;
}

static const weivel_standard_log LIBRARY_LOG = {
    .size = sizeof(weivel_standard_log),
    .context = NULL,
    .write = library_log_write,
    .release_refusal = stub_release_refusal,
};

const void *stub_host_log_definition(void) {
    return &LIBRARY_LOG;
}

const weivel_standard_description *stub_host_log_description(void) {
    return weivel_standard_log_description();
}
