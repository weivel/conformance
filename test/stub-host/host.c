#include "run.h"
#include "stub_host.h"

#include "weivel/descriptions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAN_FIRST_SHAPE (offsetof(weivel_standard_host_plan, storage) + sizeof(void *))
#define NEXT_FIRST_SHAPE (offsetof(weivel_standard_host_next, storage) + sizeof(void *))
#define DETAIL_CAPACITY 256

static const char *const STATE_NAMES[] = {"activated", "refused", "failed"};

static stub_run *current_run;

/** Whether a run is open: `start` answered OK and `stop` has not been called since, so another `start` is a mistake. */
static int run_open;

/** What a `next` answer holds as its storage, which owns nothing, so a second use of one answer is still caught. */
static char next_marker;

stub_run *stub_host_current_run(void) {
    return current_run;
}

static void record_and_free(const stub_run *run, stub_line *line) {
    if (line->text != NULL) {
        stub_record_line(run->record_path, line->text);
    }
    stub_line_free(line);
}

static void record_text(const stub_run *run, const char *text) {
    stub_record_line(run->record_path, text);
}

static void record_start(const stub_run *run, const char *const *arguments, size_t argument_count) {
    stub_line line = {NULL, 0, 0};
    size_t at;
    stub_line_add(&line, "start");
    for (at = 1; at < argument_count; at++) {
        stub_line_add(&line, " ");
        stub_line_add_quoted(&line, arguments[at], strlen(arguments[at]));
    }
    record_and_free(run, &line);
}

/** One block holding the plan's libraries, then its plugins, both pointing into the run, which outlives the plan. */
static weivel_status fill_plan(const stub_run *run, weivel_standard_host_plan *plan) {
    size_t libraries_size = run->library_count * sizeof(weivel_standard_host_library);
    size_t plugins_size = run->plugin_count * sizeof(weivel_standard_host_plugin);
    char *block = malloc(libraries_size + plugins_size + 1);
    weivel_standard_host_library *libraries = (weivel_standard_host_library *)block;
    weivel_standard_host_plugin *plugins = (weivel_standard_host_plugin *)(block + libraries_size);
    size_t at;
    if (block == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < run->library_count; at++) {
        libraries[at] = run->libraries[at];
    }
    for (at = 0; at < run->plugin_count; at++) {
        plugins[at].directory = run->plugins[at].directory;
        plugins[at].manifest = run->plugins[at].manifest;
        plugins[at].entry = run->plugins[at].entry;
    }
    stub_clear_beyond(plan, plan->size, sizeof *plan);
    plan->mode = run->mode;
    plan->libraries = run->library_count == 0 ? NULL : libraries;
    plan->library_count = run->library_count;
    plan->plugins = run->plugin_count == 0 ? NULL : plugins;
    plan->plugin_count = run->plugin_count;
    plan->action_plugin = run->action_plugin == NULL ? NULL : run->action_plugin->manifest;
    plan->action_name = run->action_name;
    plan->action_arguments = run->action_arguments;
    plan->storage = block;
    return WEIVEL_STATUS_OK;
}

static int are_arguments_well_made(const char *const *arguments, size_t argument_count) {
    size_t at;
    if (arguments == NULL) {
        return argument_count == 0;
    }
    for (at = 0; at < argument_count; at++) {
        if (arguments[at] == NULL) {
            return 0;
        }
    }
    return 1;
}

void stub_host_forget_run(void) {
    stub_lock();
    stub_run_free(current_run);
    current_run = NULL;
    run_open = 0;
    stub_unlock();
}

/** Makes `run` the current run, freeing the one an earlier run read, whose answers ended with its `stop`. */
static void replace_current_run(stub_run *run) {
    stub_run_free(current_run);
    current_run = run;
}

static weivel_status start_run(const char *const *arguments, size_t argument_count, weivel_standard_host_plan *plan,
                               weivel_standard_service_refusal *refusal) {
    char detail[DETAIL_CAPACITY];
    stub_run *run = NULL;
    weivel_status status = stub_run_read(arguments[0], &run, detail, sizeof detail);
    if (status == WEIVEL_STATUS_OUT_OF_MEMORY) {
        return status;
    }
    if (status != WEIVEL_STATUS_OK) {
        return stub_refuse(refusal, detail, "write a stub-host.json the kit's LAYER.md describes into the directory");
    }
    if (!stub_record_begin(run->record_path)) {
        stub_run_free(run);
        return stub_refuse(refusal, "cannot write record.txt in the directory", "hand start a writable directory");
    }
    stub_scope_prepare(run);
    replace_current_run(run);
    record_start(run, arguments, argument_count);
    if (run->start_refusal != NULL) {
        return stub_refuse(refusal, run->start_refusal, "remove refuse.start from stub-host.json");
    }
    return fill_plan(run, plan);
}

static weivel_status host_start(void *context, const char *const *arguments, size_t argument_count,
                                weivel_standard_host_plan *plan, weivel_standard_service_refusal *refusal) {
    weivel_status status;
    (void)context;
    if (!are_arguments_well_made(arguments, argument_count) || plan == NULL ||
        !stub_is_empty_answer(plan->size, PLAN_FIRST_SHAPE, plan->storage) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    if (run_open) {
        record_text(current_run, "start again");
        status = WEIVEL_STATUS_MALFORMED;
    } else if (argument_count == 0) {
        status = stub_refuse(refusal, "start was handed no argument, so it names no directory",
                             "pass the directory holding stub-host.json as the first argument");
    } else {
        status = start_run(arguments, argument_count, plan, refusal);
        run_open = status == WEIVEL_STATUS_OK;
    }
    stub_unlock();
    return status;
}

static weivel_status refuse_without_run(weivel_standard_service_refusal *refusal) {
    return stub_refuse(refusal, "start has not read a run", "call start first, and call this only once it answered OK");
}

static void record_outcome(const stub_run *run, const weivel_standard_host_outcome *outcome) {
    const stub_plugin *plugin = stub_run_plugin_by_manifest(run, outcome->manifest);
    stub_line line = {NULL, 0, 0};
    char state[32];
    if (outcome->state >= WEIVEL_STANDARD_HOST_ACTIVATED && outcome->state <= WEIVEL_STANDARD_HOST_FAILED) {
        snprintf(state, sizeof state, "%s", STATE_NAMES[outcome->state - 1]);
    } else {
        snprintf(state, sizeof state, "state-%lu", (unsigned long)outcome->state);
    }
    stub_line_add(&line, "outcome ");
    stub_line_add(&line, plugin == NULL ? "unknown" : plugin->name);
    stub_line_add(&line, " ");
    stub_line_add(&line, state);
    stub_line_add(&line, outcome->detail != NULL && outcome->detail[0] != '\0' ? " detail" : " no-detail");
    record_and_free(run, &line);
}

static weivel_status host_started(void *context, const weivel_standard_host_outcome *outcomes, size_t outcome_count,
                                  weivel_standard_service_refusal *refusal) {
    char line[48];
    size_t at;
    weivel_status status = WEIVEL_STATUS_OK;
    (void)context;
    if ((outcomes == NULL && outcome_count != 0) || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    if (current_run == NULL) {
        status = refuse_without_run(refusal);
    } else {
        snprintf(line, sizeof line, "started %lu", (unsigned long)outcome_count);
        record_text(current_run, line);
        for (at = 0; at < outcome_count; at++) {
            record_outcome(current_run, &outcomes[at]);
        }
        if (current_run->started_refusal != NULL) {
            status = stub_refuse(refusal, current_run->started_refusal, "remove refuse.started from stub-host.json");
        }
    }
    stub_unlock();
    return status;
}

/** How this tick came against the time the previous one answered, as the record names it. */
static const char *tick_timing(stub_run *run, int64_t now) {
    if (run->finished_ticking) {
        return "unasked";
    }
    if (run->ticks_called == 1) {
        run->first_now = now;
        return "first";
    }
    return now >= run->due ? "on-time" : "early";
}

/** Any `has_next` beyond 1 is malformed, so 2 stands for all of them. */
#define MALFORMED_HAS_NEXT 2

static void answer_next(stub_run *run, weivel_standard_host_next *next) {
    stub_clear_beyond(next, next->size, sizeof *next);
    if (run->ticks_called == run->malformed_tick) {
        next->has_next = MALFORMED_HAS_NEXT;
        next->next = 0;
    } else if (!run->finished_ticking && run->ticks_answered < run->tick_count) {
        run->due = run->first_now + run->ticks[run->ticks_answered++];
        next->has_next = 1;
        next->next = run->due;
    } else {
        run->finished_ticking = 1;
        next->has_next = 0;
        next->next = 0;
    }
    next->storage = &next_marker;
}

static weivel_status host_tick(void *context, int64_t now, weivel_standard_host_next *next,
                               weivel_standard_service_refusal *refusal) {
    char line[64];
    weivel_status status = WEIVEL_STATUS_OK;
    (void)context;
    if (next == NULL || !stub_is_empty_answer(next->size, NEXT_FIRST_SHAPE, next->storage) ||
        !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    if (current_run == NULL) {
        status = refuse_without_run(refusal);
    } else {
        current_run->ticks_called++;
        snprintf(line, sizeof line, "tick %lu %s", (unsigned long)current_run->ticks_called,
                 tick_timing(current_run, now));
        record_text(current_run, line);
        if (current_run->ticks_called == current_run->refused_tick) {
            status = stub_refuse(refusal, "the stub host was told to refuse this tick",
                                 "remove refuse.tick from stub-host.json");
        } else {
            answer_next(current_run, next);
        }
    }
    stub_unlock();
    return status;
}

static void record_finished(const stub_run *run, const weivel_standard_value *result, const char *failure) {
    stub_line line = {NULL, 0, 0};
    if ((result == NULL) == (failure == NULL)) {
        stub_line_add(&line, "finished malformed");
    } else if (failure != NULL) {
        stub_line_add(&line, "finished failure");
    } else if (result->kind == WEIVEL_STANDARD_VALUE_NONE) {
        stub_line_add(&line, "finished none");
    } else {
        stub_line_add(&line, "finished result ");
        stub_line_add_value(&line, result);
    }
    record_and_free(run, &line);
}

static weivel_status host_finished(void *context, const weivel_standard_value *result, const char *failure,
                                   weivel_standard_service_refusal *refusal) {
    weivel_status status = WEIVEL_STATUS_OK;
    (void)context;
    if (!stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    if (current_run == NULL) {
        status = refuse_without_run(refusal);
    } else {
        record_finished(current_run, result, failure);
        if ((result == NULL) == (failure == NULL)) {
            status = WEIVEL_STATUS_MALFORMED;
        }
    }
    stub_unlock();
    return status;
}

static weivel_status host_stop(void *context, uint32_t *exit_code, weivel_standard_service_refusal *refusal) {
    weivel_status status = WEIVEL_STATUS_OK;
    (void)context;
    if (exit_code == NULL || !stub_is_usable_refusal(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    stub_lock();
    if (current_run == NULL) {
        status = refuse_without_run(refusal);
    } else {
        record_text(current_run, "stop");
        *exit_code = current_run->exit_code;
        run_open = 0;
    }
    stub_unlock();
    return status;
}

static void host_release_plan(void *context, weivel_standard_host_plan *plan) {
    (void)context;
    if (plan == NULL || plan->size < PLAN_FIRST_SHAPE || plan->storage == NULL) {
        return;
    }
    free(plan->storage);
    plan->mode = 0;
    plan->libraries = NULL;
    plan->library_count = 0;
    plan->plugins = NULL;
    plan->plugin_count = 0;
    plan->action_plugin = NULL;
    plan->action_name = NULL;
    memset(&plan->action_arguments, 0, sizeof plan->action_arguments);
    plan->storage = NULL;
}

static void host_release_next(void *context, weivel_standard_host_next *next) {
    (void)context;
    if (next == NULL || next->size < NEXT_FIRST_SHAPE || next->storage == NULL) {
        return;
    }
    next->has_next = 0;
    next->next = 0;
    next->storage = NULL;
}

static const weivel_standard_host HOST = {
    .size = sizeof(weivel_standard_host),
    .context = NULL,
    .start = host_start,
    .started = host_started,
    .tick = host_tick,
    .finished = host_finished,
    .stop = host_stop,
    .release_plan = host_release_plan,
    .release_next = host_release_next,
    .release_refusal = stub_release_refusal,
};

const void *stub_host_definition(void) {
    return &HOST;
}

const weivel_standard_description *stub_host_description(void) {
    return weivel_standard_host_description();
}
