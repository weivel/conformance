#ifndef CONFORMANCE_STUB_HOST_RUN_H
#define CONFORMANCE_STUB_HOST_RUN_H

/**
 * @file
 * One run of the stub host as `stub-host.json` states it, and the modules that share it: the run file's reader, the
 * record, the answers both definitions fill, and the lock every call takes, since a provider is called from any
 * thread (`weivel/services.h`, Threads).
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/host.h"
#include "weivel/json.h"
#include "weivel/scope.h"
#include "weivel/services.h"
#include "weivel/value.h"

typedef struct stub_setting {
    char *key;
    weivel_standard_setting_value value;
} stub_setting;

typedef struct stub_secret {
    char *name;
    uint8_t *bytes;
    size_t length;
} stub_secret;

/** Every member lives as long as the run, so a definition answered for the plugin stays valid across later calls. */
typedef struct stub_plugin {
    char *name;
    char *directory;
    char *manifest;
    char *entry;
    char *cache_directory;
    char *config_directory;
    int scoped;
    /** The detail `open` refuses the plugin's scope with, or NULL when it opens. */
    char *scope_refusal;
    stub_setting *settings;
    size_t setting_count;
    stub_secret *secrets;
    size_t secret_count;
    weivel_standard_config config;
    weivel_standard_secrets secrets_definition;
    weivel_standard_paths paths;
    weivel_standard_log log;
} stub_plugin;

typedef struct stub_run {
    char *directory;
    char *record_path;
    uint32_t mode;
    weivel_standard_host_library *libraries;
    size_t library_count;
    stub_plugin *plugins;
    size_t plugin_count;
    const stub_plugin *action_plugin;
    char *action_name;
    weivel_standard_value action_arguments;
    /** Milliseconds after the first tick's `now`, answered one per tick in order. */
    int64_t *ticks;
    size_t tick_count;
    size_t ticks_answered;
    size_t ticks_called;
    int64_t first_now;
    int64_t due;
    int finished_ticking;
    uint32_t exit_code;
    char *start_refusal;
    char *started_refusal;
    /** The number of the tick that refuses, counting from 1, or 0 when none does. */
    size_t refused_tick;
} stub_run;

/**
 * Reads `<directory>/stub-host.json` into a run. Answers OK, or REFUSED with `detail` saying what the file breaks, or
 * OUT_OF_MEMORY.
 */
weivel_status stub_run_read(const char *directory, stub_run **run, char *detail, size_t detail_size);
void stub_run_free(stub_run *run);
/** The plugin whose manifest path is `manifest`, as the plan named it, or NULL. */
stub_plugin *stub_run_plugin_by_manifest(const stub_run *run, const char *manifest);

/** Gives each plugin of the run its config, secrets, paths and log, each with the plugin as its context. */
void stub_scope_prepare(stub_run *run);

/** The run `start` read, or NULL before one was read. */
stub_run *stub_host_current_run(void);
/** Frees the current run and closes it, as a fresh process would have none; for the kit's own tests. */
void stub_host_forget_run(void);

void stub_lock(void);
void stub_unlock(void);

/** Empties `<directory>/record.txt`; 0 when it cannot be written. */
int stub_record_begin(const char *record_path);
/** Appends one line; the caller holds the lock, so lines from two threads never interleave. */
void stub_record_line(const char *record_path, const char *line);

/**
 * A growing line of the record. Its text is NULL once memory ran out, and appending to it is then a no-op, so a caller
 * checks once, before writing it.
 */
typedef struct stub_line {
    char *text;
    size_t length;
    size_t capacity;
} stub_line;

void stub_line_add(stub_line *line, const char *text);
void stub_line_add_quoted(stub_line *line, const char *text, size_t length);
void stub_line_add_value(stub_line *line, const weivel_standard_value *value);
void stub_line_add_setting(stub_line *line, const weivel_standard_setting_value *value);
void stub_line_free(stub_line *line);

/** Copies `json` into a value tree the caller frees with stub_value_free; 0 when memory ran out or a text holds NUL. */
int stub_value_from_json(const weivel_json_value *json, weivel_standard_value *value);
void stub_value_free(weivel_standard_value *value);

char *stub_copy_text(const char *text);
/** Whether an answer or refusal of `size` reaches `first_shape` and holds no storage. */
int stub_is_empty_answer(uint32_t size, size_t first_shape, const void *storage);
/** Zeroes what a newer consumer's answer of `size` holds beyond the provider's `provider_size`. */
void stub_clear_beyond(void *answer, uint32_t size, size_t provider_size);
/** Whether `refusal` is NULL, or empty and at least its first shape. */
int stub_is_usable_refusal(const weivel_standard_service_refusal *refusal);
/** Answers REFUSED, filling `refusal` with `detail` and `fix` when one was handed in. */
weivel_status stub_refuse(weivel_standard_service_refusal *refusal, const char *detail, const char *fix);
void stub_release_refusal(void *context, weivel_standard_service_refusal *refusal);

#endif
