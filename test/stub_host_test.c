#include "check.h"
#include "run.h"
#include "stub_host.h"

#include "weivel/descriptions.h"
#include "weivel/host.h"
#include "weivel/library.h"
#include "weivel/scope.h"

#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define LINE_CAPACITY 64
#define RECORD_CAPACITY 8192

static const weivel_standard_host *host(void) {
    return stub_host_definition();
}

static const weivel_standard_scopes *scopes(void) {
    return stub_host_scopes_definition();
}

static void write_run(const char *json) {
    char path[512];
    FILE *file;
    snprintf(path, sizeof path, "%s/stub-host.json", STUB_HOST_WORK_DIR);
    file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        fputs(json, file);
        fclose(file);
    }
}

static void work_path(const char *relative, char *path, size_t size) {
    snprintf(path, size, "%s/%s", STUB_HOST_WORK_DIR, relative);
}

/** Checks record.txt holds exactly `expected`, `count` lines, in order. */
static void check_record(const char *const *expected, size_t count, const char *label) {
    char path[512];
    char text[RECORD_CAPACITY];
    char want[RECORD_CAPACITY] = "";
    size_t length = 0;
    size_t at;
    FILE *file;
    work_path("record.txt", path, sizeof path);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        length = fread(text, 1, sizeof text - 1, file);
        fclose(file);
    }
    text[length] = '\0';
    for (at = 0; at < count; at++) {
        strncat(want, expected[at], sizeof want - strlen(want) - 2);
        strncat(want, "\n", sizeof want - strlen(want) - 1);
    }
    check_text(text, want, label);
}

/** Calls `start` as a fresh process would, forgetting the run an earlier scenario started. */
static weivel_status start_with(const char *const *arguments, size_t count, weivel_standard_host_plan *plan,
                                weivel_standard_service_refusal *refusal) {
    stub_host_forget_run();
    return host()->start(host()->context, arguments, count, plan, refusal);
}

static weivel_status start_in_work(weivel_standard_host_plan *plan, weivel_standard_service_refusal *refusal) {
    const char *arguments[] = {STUB_HOST_WORK_DIR};
    return start_with(arguments, 1, plan, refusal);
}

static const char SERVE_RUN[] =
    "{\"mode\":\"serve\","
    "\"libraries\":[{\"binary\":\"lib/one.bin\",\"declaration\":\"/elsewhere/library.json\"}],"
    "\"plugins\":["
    "{\"name\":\"gamma\",\"directory\":\"gamma\",\"manifest\":\"gamma/manifest.json\",\"entry\":\"gamma/entry\","
    "\"scope\":true,\"settings\":{\"greeting\":\"hello\",\"count\":3,\"on\":true}},"
    "{\"name\":\"alpha\",\"directory\":\"alpha\",\"manifest\":\"alpha/manifest.json\",\"entry\":\"alpha/entry\"},"
    "{\"name\":\"beta\",\"directory\":\"beta\",\"manifest\":\"beta/manifest.json\",\"entry\":\"beta/entry\","
    "\"scopeRefused\":\"beta declares a setting of no known kind\"}],"
    "\"ticks\":[50,120],\"exit\":7}";

static void a_serve_run_answers_its_plan_in_order(void) {
    const char *arguments[] = {STUB_HOST_WORK_DIR, "--extra", "two words"};
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    char path[512];
    static const char *const RECORD[] = {"start \"--extra\" \"two words\""};
    write_run(SERVE_RUN);
    CHECK(start_with(arguments, 3, &plan, NULL) == WEIVEL_STATUS_OK);
    CHECK(plan.mode == WEIVEL_STANDARD_HOST_SERVE);
    CHECK(plan.library_count == 1 && plan.plugin_count == 3);
    if (plan.library_count == 1 && plan.plugin_count == 3) {
        work_path("lib/one.bin", path, sizeof path);
        check_text(plan.libraries[0].binary, path, "a relative binary joins the directory");
        check_text(plan.libraries[0].declaration, "/elsewhere/library.json", "an absolute path is kept");
        work_path("gamma/manifest.json", path, sizeof path);
        check_text(plan.plugins[0].manifest, path, "the first plugin is the first listed");
        work_path("alpha", path, sizeof path);
        check_text(plan.plugins[1].directory, path, "the second plugin's directory");
        work_path("beta/entry", path, sizeof path);
        check_text(plan.plugins[2].entry, path, "the third plugin's entry");
    }
    CHECK(plan.action_plugin == NULL && plan.action_name == NULL);
    CHECK(plan.action_arguments.kind == WEIVEL_STANDARD_VALUE_NONE);
    CHECK(plan.storage != NULL);
    CHECK(start_with(arguments, 3, &plan, NULL) == WEIVEL_STATUS_MALFORMED);
    host()->release_plan(host()->context, &plan);
    CHECK(plan.storage == NULL && plan.plugins == NULL && plan.size == sizeof plan);
    check_record(RECORD, 1, "start records the arguments after the directory");
}

static void an_action_run_answers_its_action(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    char path[512];
    write_run("{\"mode\":\"action\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"alpha\","
              "\"manifest\":\"alpha/manifest.json\",\"entry\":\"alpha/entry\"}],"
              "\"action\":{\"plugin\":\"alpha\",\"name\":\"echo\",\"arguments\":{\"text\":\"hi\",\"times\":[1,2]}}}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    CHECK(plan.mode == WEIVEL_STANDARD_HOST_ACTION);
    work_path("alpha/manifest.json", path, sizeof path);
    check_text(plan.action_plugin, path, "the action names its plugin's manifest");
    check_text(plan.action_name, "echo", "the action's name");
    CHECK(plan.action_service == NULL && plan.action_method == NULL);
    CHECK(plan.action_arguments.kind == WEIVEL_STANDARD_VALUE_RECORD);
    CHECK(plan.action_arguments.count == 2);
    if (plan.action_arguments.count == 2) {
        check_text(plan.action_arguments.entries[0].key, "text", "the first argument's key");
        check_text(plan.action_arguments.entries[0].value.text, "hi", "the first argument's text");
        CHECK(plan.action_arguments.entries[1].value.kind == WEIVEL_STANDARD_VALUE_LIST);
        CHECK(plan.action_arguments.entries[1].value.count == 2);
    }
    host()->release_plan(host()->context, &plan);
}

static const char ONE_PLUGIN_ACTION_RUN[] =
    "{\"mode\":\"action\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"alpha\","
    "\"manifest\":\"alpha/manifest.json\",\"entry\":\"alpha/entry\"}],";

static void write_action_run(const char *action) {
    char json[1024];
    snprintf(json, sizeof json, "%s\"action\":%s}", ONE_PLUGIN_ACTION_RUN, action);
    write_run(json);
}

static void an_action_run_answers_the_service_method_it_names(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    write_action_run("{\"plugin\":\"alpha\",\"name\":\"login\",\"service\":\"alpha:accounts\",\"method\":\"login\","
                     "\"arguments\":\"someone\"}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    check_text(plan.action_service, "alpha:accounts", "the action's service");
    check_text(plan.action_method, "login", "the action's method");
    check_text(plan.action_name, "login", "a name given beside a method is answered too");
    CHECK(plan.action_arguments.kind == WEIVEL_STANDARD_VALUE_TEXT);
    host()->release_plan(host()->context, &plan);
    CHECK(plan.action_service == NULL && plan.action_method == NULL && plan.storage == NULL);
    write_action_run("{\"plugin\":\"alpha\",\"service\":\"alpha:accounts\",\"method\":\"login\"}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    CHECK(plan.action_name == NULL);
    check_text(plan.action_method, "login", "a method needs no name beside it");
    host()->release_plan(host()->context, &plan);
}

static void one_of_service_and_method_is_answered_as_given(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    write_action_run("{\"plugin\":\"alpha\",\"service\":\"\",\"method\":\"login\"}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    check_text(plan.action_service, "", "an empty service is answered as given");
    host()->release_plan(host()->context, &plan);
    write_action_run("{\"plugin\":\"alpha\",\"service\":\"alpha:accounts\"}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    check_text(plan.action_service, "alpha:accounts", "a service without a method is answered");
    CHECK(plan.action_method == NULL && plan.action_name == NULL);
    host()->release_plan(host()->context, &plan);
    write_action_run("{\"plugin\":\"alpha\",\"method\":\"login\"}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    check_text(plan.action_method, "login", "a method without a service is answered");
    CHECK(plan.action_service == NULL && plan.action_name == NULL);
    host()->release_plan(host()->context, &plan);
}

/** What a consumer compiled before `action_service` holds of its plan, with room after it to see nothing is written. */
typedef struct older_plan {
    weivel_standard_host_plan plan;
    char beyond[sizeof(const char *) * 2];
} older_plan;

static int is_untouched(const older_plan *older, size_t from) {
    const unsigned char *bytes = (const unsigned char *)older;
    size_t at;
    for (at = from; at < sizeof *older; at++) {
        if (bytes[at] != 0xA5) {
            return 0;
        }
    }
    return 1;
}

static void start_older(older_plan *older, size_t older_size) {
    memset(older, 0xA5, sizeof *older);
    memset(&older->plan, 0, older_size);
    older->plan.size = (uint32_t)older_size;
}

static void a_plan_from_an_older_consumer_is_written_only_within_its_size(void) {
    older_plan older;
    size_t older_size = offsetof(weivel_standard_host_plan, action_service);
    start_older(&older, older_size);
    write_action_run("{\"plugin\":\"alpha\",\"name\":\"echo\"}");
    CHECK(start_in_work(&older.plan, NULL) == WEIVEL_STATUS_OK);
    CHECK(older.plan.size == older_size && older.plan.storage != NULL);
    check_text(older.plan.action_name, "echo", "an older plan still carries a named action");
    CHECK(is_untouched(&older, older_size));
    host()->release_plan(host()->context, &older.plan);
    CHECK(older.plan.storage == NULL);
    CHECK(is_untouched(&older, older_size));
}

static void a_method_run_refuses_a_plan_too_old_to_hold_it(void) {
    static const char *const RUNS[] = {"{\"plugin\":\"alpha\",\"service\":\"alpha:accounts\",\"method\":\"login\"}",
                                       "{\"plugin\":\"alpha\",\"service\":\"alpha:accounts\"}"};
    static const size_t SIZES[] = {offsetof(weivel_standard_host_plan, action_service),
                                   offsetof(weivel_standard_host_plan, action_method)};
    size_t run;
    size_t size;
    for (run = 0; run < sizeof RUNS / sizeof RUNS[0]; run++) {
        for (size = 0; size < sizeof SIZES / sizeof SIZES[0]; size++) {
            older_plan older;
            weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
            if (run == 1 && size == 1) {
                continue;
            }
            start_older(&older, SIZES[size]);
            write_action_run(RUNS[run]);
            CHECK(start_in_work(&older.plan, &refusal) == WEIVEL_STATUS_REFUSED);
            CHECK(older.plan.storage == NULL);
            CHECK(is_untouched(&older, SIZES[size]));
            CHECK(refusal.detail != NULL && strstr(refusal.detail, "too old") != NULL);
            host()->release_refusal(host()->context, &refusal);
        }
    }
}

static void check_start_refused(const char *json, const char *needle) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    write_run(json);
    CHECK(start_in_work(&plan, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(plan.storage == NULL);
    if (refusal.detail == NULL || strstr(refusal.detail, needle) == NULL) {
        fprintf(stderr, "expected a refusal naming \"%s\", got \"%s\"\n", needle,
                refusal.detail == NULL ? "(none)" : refusal.detail);
        check_failures++;
    }
    CHECK(refusal.fix != NULL);
    host()->release_refusal(host()->context, &refusal);
    CHECK(refusal.storage == NULL && refusal.detail == NULL);
}

static void a_run_file_that_breaks_its_shape_refuses_start(void) {
    check_start_refused("{\"plugins\":[]}", "mode");
    check_start_refused("{\"mode\":\"serve\"}", "plugins");
    check_start_refused("not json", "not JSON");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"a\",\"manifest\":\"m\","
                        "\"entry\":\"e\",\"settings\":{\"list\":[1]}}]}",
                        "settings.list");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"a\","
                        "\"manifest\":\"m\"}]}",
                        "entry");
    check_start_refused("{\"mode\":\"action\",\"plugins\":[]}", "action");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"ticks\":[-1]}", "ticks[0]");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"exit\":256}", "exit");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"ticks\":[40,40]}", "ticks[1]");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"refuse\":{\"tick\":0}}", "refuse.tick");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"malformedTick\":0}", "malformedTick");
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"malformedTick\":\"1\"}", "malformedTick");
    check_start_refused("{\"mode\":\"action\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"a\",\"manifest\":\"m\","
                        "\"entry\":\"e\"}],\"action\":{\"plugin\":\"alpha\"}}",
                        "action.name");
    check_start_refused("{\"mode\":\"action\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"a\",\"manifest\":\"m\","
                        "\"entry\":\"e\"}],\"action\":{\"plugin\":\"alpha\",\"service\":7,\"method\":\"login\"}}",
                        "action.service");
    check_start_refused("{\"mode\":\"action\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"a\",\"manifest\":\"m\","
                        "\"entry\":\"e\"}],\"action\":{\"plugin\":\"alpha\",\"name\":\"\"}}",
                        "action.name");
    check_start_refused("{\"mode\":\"action\",\"plugins\":[{\"name\":\"alpha\",\"directory\":\"a\",\"manifest\":\"m\","
                        "\"entry\":\"e\"}],\"action\":{\"plugin\":\"alpha\",\"service\":\"alpha:a\",\"method\":3}}",
                        "action.method");
}

static void a_start_told_to_refuse_records_only_itself(void) {
    static const char *const RECORD[] = {"start"};
    check_start_refused("{\"mode\":\"serve\",\"plugins\":[],\"refuse\":{\"start\":\"told to refuse\"}}",
                        "told to refuse");
    check_record(RECORD, 1, "a refused start records start alone");
}

static void start_refuses_a_call_that_is_not_well_made(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    const char *missing[] = {NULL};
    CHECK(start_with(NULL, 1, &plan, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(start_with(missing, 1, &plan, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(start_in_work(NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    plan.size = 8;
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_MALFORMED);
    plan.size = sizeof plan;
    CHECK(start_with(NULL, 0, &plan, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.detail != NULL);
    host()->release_refusal(host()->context, &refusal);
}

static void start_serve_run(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    write_run(SERVE_RUN);
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    host()->release_plan(host()->context, &plan);
}

static void plugin_manifest(const char *name, char *path, size_t size) {
    char relative[64];
    snprintf(relative, sizeof relative, "%s/manifest.json", name);
    work_path(relative, path, size);
}

static void started_records_each_outcome_in_order(void) {
    char gamma[512];
    char beta[512];
    weivel_standard_host_outcome outcomes[3];
    static const char *const RECORD[] = {"start", "started 3", "outcome gamma activated no-detail",
                                         "outcome beta refused detail", "outcome unknown failed detail"};
    start_serve_run();
    plugin_manifest("gamma", gamma, sizeof gamma);
    plugin_manifest("beta", beta, sizeof beta);
    outcomes[0].manifest = gamma;
    outcomes[0].state = WEIVEL_STANDARD_HOST_ACTIVATED;
    outcomes[0].detail = NULL;
    outcomes[1].manifest = beta;
    outcomes[1].state = WEIVEL_STANDARD_HOST_REFUSED;
    outcomes[1].detail = "its scope was refused";
    outcomes[2].manifest = "/nowhere/manifest.json";
    outcomes[2].state = WEIVEL_STANDARD_HOST_FAILED;
    outcomes[2].detail = "it threw";
    CHECK(host()->started(host()->context, outcomes, 3, NULL) == WEIVEL_STATUS_OK);
    CHECK(host()->started(host()->context, NULL, 1, NULL) == WEIVEL_STATUS_MALFORMED);
    check_record(RECORD, 5, "started records its outcomes");
}

static void started_told_to_refuse_refuses(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    write_run("{\"mode\":\"serve\",\"plugins\":[],\"refuse\":{\"started\":\"will not run on\"}}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    host()->release_plan(host()->context, &plan);
    CHECK(host()->started(host()->context, NULL, 0, &refusal) == WEIVEL_STATUS_REFUSED);
    check_text(refusal.detail, "will not run on", "started's refusal");
    host()->release_refusal(host()->context, &refusal);
}

static void tick_once(int64_t now, uint32_t has_next, int64_t next_time) {
    weivel_standard_host_next next = WEIVEL_INIT(weivel_standard_host_next);
    CHECK(host()->tick(host()->context, now, &next, NULL) == WEIVEL_STATUS_OK);
    CHECK(next.has_next == has_next);
    CHECK(next.next == next_time);
    CHECK(host()->tick(host()->context, now, &next, NULL) == WEIVEL_STATUS_MALFORMED);
    host()->release_next(host()->context, &next);
    CHECK(next.storage == NULL && next.has_next == 0);
}

static void tick_answers_each_listed_time_after_the_first_then_none(void) {
    static const char *const RECORD[] = {"start", "tick 1 first", "tick 2 on-time", "tick 3 early", "tick 4 unasked"};
    start_serve_run();
    tick_once(1000, 1, 1050);
    tick_once(1050, 1, 1120);
    tick_once(1119, 0, 0);
    tick_once(5000, 0, 0);
    check_record(RECORD, 5, "tick records how each call came");
}

static void a_second_start_is_the_callers_mistake(void) {
    weivel_standard_host_plan second = WEIVEL_INIT(weivel_standard_host_plan);
    const char *arguments[] = {STUB_HOST_WORK_DIR};
    static const char *const RECORD[] = {"start", "start again"};
    start_serve_run();
    CHECK(host()->start(host()->context, arguments, 1, &second, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(second.storage == NULL);
    check_record(RECORD, 2, "a second start is recorded and refused");
}

static void a_start_after_a_stop_opens_a_new_run(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    const char *arguments[] = {STUB_HOST_WORK_DIR};
    uint32_t exit_code = 0;
    static const char *const RECORD[] = {"start", "stop"};
    start_serve_run();
    CHECK(host()->stop(host()->context, &exit_code, NULL) == WEIVEL_STATUS_OK);
    write_run("{\"mode\":\"serve\",\"plugins\":[],\"exit\":4}");
    CHECK(host()->start(host()->context, arguments, 1, &plan, NULL) == WEIVEL_STATUS_OK);
    host()->release_plan(host()->context, &plan);
    CHECK(host()->stop(host()->context, &exit_code, NULL) == WEIVEL_STATUS_OK);
    CHECK(exit_code == 4);
    check_record(RECORD, 2, "the second run records afresh");
}

static void a_start_after_a_refused_start_opens_a_run(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    const char *arguments[] = {STUB_HOST_WORK_DIR};
    write_run("{\"mode\":\"serve\",\"plugins\":[],\"refuse\":{\"start\":\"once\"}}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_REFUSED);
    CHECK(host()->start(host()->context, NULL, 0, &plan, NULL) == WEIVEL_STATUS_REFUSED);
    write_run("{\"mode\":\"serve\",\"plugins\":[]}");
    CHECK(host()->start(host()->context, arguments, 1, &plan, NULL) == WEIVEL_STATUS_OK);
    host()->release_plan(host()->context, &plan);
}

static void a_tick_told_to_refuse_refuses(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    weivel_standard_host_next next = WEIVEL_INIT(weivel_standard_host_next);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    static const char *const RECORD[] = {"start", "tick 1 first", "tick 2 on-time"};
    write_run("{\"mode\":\"serve\",\"plugins\":[],\"ticks\":[10,20],\"refuse\":{\"tick\":2}}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    host()->release_plan(host()->context, &plan);
    tick_once(100, 1, 110);
    CHECK(host()->tick(host()->context, 110, &next, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(next.storage == NULL && next.has_next == 0);
    CHECK(refusal.detail != NULL);
    host()->release_refusal(host()->context, &refusal);
    check_record(RECORD, 3, "the refused tick is recorded before it refuses");
}

static void a_malformed_tick_answers_a_has_next_beyond_one(void) {
    weivel_standard_host_plan plan = WEIVEL_INIT(weivel_standard_host_plan);
    weivel_standard_host_next next = WEIVEL_INIT(weivel_standard_host_next);
    static const char *const RECORD[] = {"start", "tick 1 first"};
    write_run("{\"mode\":\"serve\",\"plugins\":[],\"ticks\":[10],\"malformedTick\":1}");
    CHECK(start_in_work(&plan, NULL) == WEIVEL_STATUS_OK);
    host()->release_plan(host()->context, &plan);
    CHECK(host()->tick(host()->context, 100, &next, NULL) == WEIVEL_STATUS_OK);
    CHECK(next.has_next > 1);
    host()->release_next(host()->context, &next);
    CHECK(next.storage == NULL);
    check_record(RECORD, 2, "the malformed tick is recorded as any other");
}

static void finished_records_a_result_none_a_failure_and_a_mistake(void) {
    weivel_standard_value none = {.kind = WEIVEL_STANDARD_VALUE_NONE};
    weivel_standard_value items[2];
    weivel_standard_entry entries[2];
    weivel_standard_value result;
    static const char *const RECORD[] = {"start",
                                         "finished none",
                                         "finished result {\"text\":\"a \\\"b\\\"\",\"list\":[true,1.5]}",
                                         "finished failure",
                                         "finished malformed",
                                         "finished malformed"};
    memset(items, 0, sizeof items);
    memset(entries, 0, sizeof entries);
    memset(&result, 0, sizeof result);
    items[0].kind = WEIVEL_STANDARD_VALUE_BOOLEAN;
    items[0].boolean = 1;
    items[1].kind = WEIVEL_STANDARD_VALUE_NUMBER;
    items[1].number = 1.5;
    entries[0].key = "text";
    entries[0].key_length = 4;
    entries[0].value.kind = WEIVEL_STANDARD_VALUE_TEXT;
    entries[0].value.text = "a \"b\"";
    entries[0].value.length = 5;
    entries[1].key = "list";
    entries[1].key_length = 4;
    entries[1].value.kind = WEIVEL_STANDARD_VALUE_LIST;
    entries[1].value.items = items;
    entries[1].value.count = 2;
    result.kind = WEIVEL_STANDARD_VALUE_RECORD;
    result.entries = entries;
    result.count = 2;
    start_serve_run();
    CHECK(host()->finished(host()->context, &none, NULL, NULL) == WEIVEL_STATUS_OK);
    CHECK(host()->finished(host()->context, &result, NULL, NULL) == WEIVEL_STATUS_OK);
    CHECK(host()->finished(host()->context, NULL, "it threw", NULL) == WEIVEL_STATUS_OK);
    CHECK(host()->finished(host()->context, NULL, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(host()->finished(host()->context, &none, "both", NULL) == WEIVEL_STATUS_MALFORMED);
    check_record(RECORD, 6, "finished records how the action ended");
}

static void stop_answers_the_exit_code(void) {
    uint32_t exit_code = 0;
    static const char *const RECORD[] = {"start", "stop"};
    start_serve_run();
    CHECK(host()->stop(host()->context, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(host()->stop(host()->context, &exit_code, NULL) == WEIVEL_STATUS_OK);
    CHECK(exit_code == 7);
    check_record(RECORD, 2, "stop is recorded");
}

static weivel_status open_scope(const char *name, weivel_standard_scope *scope,
                                weivel_standard_service_refusal *refusal) {
    char manifest[512];
    plugin_manifest(name, manifest, sizeof manifest);
    return scopes()->open(scopes()->context, manifest, scope, refusal);
}

static void a_scoped_plugin_reads_and_writes_its_own_config(const weivel_standard_config *config) {
    weivel_standard_setting_answer answer = WEIVEL_INIT(weivel_standard_setting_answer);
    weivel_standard_settings settings = WEIVEL_INIT(weivel_standard_settings);
    weivel_standard_setting_value value = {.kind = WEIVEL_STANDARD_SETTING_NONE};
    CHECK(config->get(config->context, "greeting", &answer, NULL) == WEIVEL_STATUS_OK);
    CHECK(answer.value.kind == WEIVEL_STANDARD_SETTING_TEXT);
    check_text(answer.value.text, "hello", "the plugin's own setting");
    config->release_answer(config->context, &answer);
    CHECK(answer.storage == NULL);
    CHECK(config->get(config->context, "count", &answer, NULL) == WEIVEL_STATUS_OK);
    CHECK(answer.value.kind == WEIVEL_STANDARD_SETTING_NUMBER && answer.value.number == 3);
    config->release_answer(config->context, &answer);
    CHECK(config->get(config->context, "absent", &answer, NULL) == WEIVEL_STATUS_NONE);
    value.kind = WEIVEL_STANDARD_SETTING_TEXT;
    value.text = "bonjour";
    value.text_length = 7;
    CHECK(config->set(config->context, "greeting", &value, NULL) == WEIVEL_STATUS_OK);
    value.kind = WEIVEL_STANDARD_SETTING_BOOLEAN;
    value.text = NULL;
    value.text_length = 0;
    value.boolean = 2;
    CHECK(config->set(config->context, "seen", &value, NULL) == WEIVEL_STATUS_MALFORMED);
    value.boolean = 1;
    CHECK(config->set(config->context, "seen", &value, NULL) == WEIVEL_STATUS_OK);
    CHECK(config->all(config->context, &settings, NULL) == WEIVEL_STATUS_OK);
    CHECK(settings.count == 4);
    if (settings.count == 4) {
        check_text(settings.settings[0].value.text, "bonjour", "a set replaces the value");
        check_text(settings.settings[3].key, "seen", "a set of a new key adds it");
    }
    config->release_settings(config->context, &settings);
    CHECK(settings.storage == NULL && settings.count == 0);
}

static void a_scoped_plugin_keeps_secrets_and_paths(const weivel_standard_secrets *secrets,
                                                    const weivel_standard_paths *paths) {
    weivel_standard_secret secret = WEIVEL_INIT(weivel_standard_secret);
    const char *directory = NULL;
    char expected[512];
    CHECK(secrets->get(secrets->context, "token", &secret, NULL) == WEIVEL_STATUS_NONE);
    CHECK(secrets->set(secrets->context, "token", (const uint8_t *)"abc", 3, NULL) == WEIVEL_STATUS_OK);
    CHECK(secrets->get(secrets->context, "token", &secret, NULL) == WEIVEL_STATUS_OK);
    CHECK(secret.length == 3 && memcmp(secret.bytes, "abc", 3) == 0);
    secrets->release_secret(secrets->context, &secret);
    CHECK(secrets->remove(secrets->context, "token", NULL) == WEIVEL_STATUS_OK);
    CHECK(secrets->get(secrets->context, "token", &secret, NULL) == WEIVEL_STATUS_NONE);
    CHECK(paths->plugin(paths->context, &directory) == WEIVEL_STATUS_OK);
    work_path("gamma", expected, sizeof expected);
    check_text(directory, expected, "the plugin's directory");
    CHECK(paths->cache(paths->context, &directory) == WEIVEL_STATUS_OK);
    work_path("gamma/cache", expected, sizeof expected);
    check_text(directory, expected, "the plugin's cache");
    CHECK(paths->config(paths->context, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void a_scoped_plugin_writes_its_log(const weivel_standard_log *log) {
    weivel_standard_value fields = {.kind = WEIVEL_STANDARD_VALUE_NONE};
    weivel_standard_entry entry;
    weivel_standard_value text = {.kind = WEIVEL_STANDARD_VALUE_NONE};
    memset(&entry, 0, sizeof entry);
    CHECK(log->write(log->context, WEIVEL_STANDARD_LOG_INFO, "up", NULL, NULL) == WEIVEL_STATUS_OK);
    text.kind = WEIVEL_STANDARD_VALUE_TEXT;
    text.text = "x";
    text.length = 1;
    entry.key = "at";
    entry.key_length = 2;
    entry.value = text;
    fields.kind = WEIVEL_STANDARD_VALUE_RECORD;
    fields.entries = &entry;
    fields.count = 1;
    CHECK(log->write(log->context, WEIVEL_STANDARD_LOG_ERROR, "line\nbreak", &fields, NULL) == WEIVEL_STATUS_OK);
    CHECK(log->write(log->context, 0, "no level", NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(log->write(log->context, WEIVEL_STANDARD_LOG_INFO, "not a record", &text, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void scope_answers_a_scoped_plugin_its_own_definitions(void) {
    weivel_standard_scope scope = WEIVEL_INIT(weivel_standard_scope);
    static const char *const RECORD[] = {"start", "open gamma scoped", "set gamma \"greeting\" \"bonjour\"",
                                         "set gamma \"seen\" true", "log gamma info \"up\"",
                                         "log gamma error \"line\\nbreak\" {\"at\":\"x\"}"};
    start_serve_run();
    CHECK(open_scope("gamma", &scope, NULL) == WEIVEL_STATUS_OK);
    CHECK(scope.config != NULL && scope.secrets != NULL && scope.paths != NULL && scope.log != NULL);
    if (scope.config != NULL && scope.secrets != NULL && scope.paths != NULL && scope.log != NULL) {
        a_scoped_plugin_reads_and_writes_its_own_config(scope.config);
        a_scoped_plugin_keeps_secrets_and_paths(scope.secrets, scope.paths);
        a_scoped_plugin_writes_its_log(scope.log);
    }
    CHECK(open_scope("gamma", &scope, NULL) == WEIVEL_STATUS_MALFORMED);
    scopes()->release_scope(scopes()->context, &scope);
    CHECK(scope.storage == NULL && scope.config == NULL);
    check_record(RECORD, 6, "a scoped plugin's calls are recorded under its name");
}

static void scope_offers_an_unscoped_plugin_nothing_and_refuses_as_told(void) {
    weivel_standard_scope scope = WEIVEL_INIT(weivel_standard_scope);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    static const char *const RECORD[] = {"start", "open alpha empty", "open beta refused", "open unknown refused"};
    start_serve_run();
    CHECK(open_scope("alpha", &scope, NULL) == WEIVEL_STATUS_OK);
    CHECK(scope.config == NULL && scope.secrets == NULL && scope.paths == NULL && scope.log == NULL);
    scopes()->release_scope(scopes()->context, &scope);
    CHECK(open_scope("beta", &scope, &refusal) == WEIVEL_STATUS_REFUSED);
    check_text(refusal.detail, "beta declares a setting of no known kind", "the refused scope's detail");
    CHECK(scope.storage == NULL);
    scopes()->release_refusal(scopes()->context, &refusal);
    CHECK(scopes()->open(scopes()->context, "/nowhere/manifest.json", &scope, NULL) == WEIVEL_STATUS_REFUSED);
    CHECK(scopes()->open(scopes()->context, NULL, &scope, NULL) == WEIVEL_STATUS_MALFORMED);
    check_record(RECORD, 4, "each open is recorded");
}

typedef void (*any_function)(void);
typedef const void *(*definition_symbol)(void);
typedef const weivel_standard_description *(*description_symbol)(void);

/** @implNote dlsym answers an object pointer, which ISO C forbids casting to a function pointer, so it is copied. */
static any_function find_function(void *module, const char *name) {
    any_function function = NULL;
#if defined(_WIN32)
    FARPROC found = GetProcAddress((HMODULE)module, name);
#else
    void *found = dlsym(module, name);
#endif
    memcpy(&function, &found, sizeof function);
    return function;
}

static char *read_declaration(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    char *text = malloc(RECORD_CAPACITY);
    *length = 0;
    if (file != NULL && text != NULL) {
        *length = fread(text, 1, RECORD_CAPACITY, file);
    }
    if (file != NULL) {
        fclose(file);
    }
    return text;
}

static void check_exported_service(void *module, const weivel_standard_library_service *service,
                                   const char *description_name, const char *definition_struct) {
    definition_symbol definition = (definition_symbol)find_function(module, service->functions[0].symbol);
    description_symbol description = (description_symbol)find_function(module, description_name);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(definition != NULL && description != NULL);
    if (definition == NULL || description == NULL) {
        return;
    }
    CHECK(definition() != NULL && *(const uint32_t *)definition() >= 8);
    CHECK(weivel_standard_description_check(description(), &refusal) == WEIVEL_STATUS_OK);
    weivel_standard_refusal_release(&refusal);
    check_text(description()->definition, definition_struct, "the description names its definition");
}

static void *open_stub_host(void) {
#if defined(_WIN32)
    return (void *)LoadLibraryA(STUB_HOST_LIBRARY);
#else
    return dlopen(STUB_HOST_LIBRARY, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void close_stub_host(void *module) {
#if defined(_WIN32)
    CHECK(module == NULL || FreeLibrary((HMODULE)module) != 0);
#else
    CHECK(module == NULL || dlclose(module) == 0);
#endif
}

static void check_library_log_keeps_nothing(void *module, const weivel_standard_library_service *service) {
    definition_symbol definition = (definition_symbol)find_function(module, service->functions[0].symbol);
    const weivel_standard_log *log = definition == NULL ? NULL : definition();
    weivel_standard_value text = {.kind = WEIVEL_STANDARD_VALUE_TEXT, .text = "x", .length = 1};
    CHECK(log != NULL);
    if (log == NULL) {
        return;
    }
    CHECK(log->write(log->context, WEIVEL_STANDARD_LOG_INFO, "kept nowhere", NULL, NULL) == WEIVEL_STATUS_OK);
    CHECK(log->write(log->context, 0, "no level", NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(log->write(log->context, WEIVEL_STANDARD_LOG_INFO, NULL, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(log->write(log->context, WEIVEL_STANDARD_LOG_INFO, "not a record", &text, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void the_log_declaration_names_a_log_the_library_exports(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    size_t length;
    char *declaration = read_declaration(STUB_HOST_LOG_DECLARATION, &length);
    void *module = open_stub_host();
    CHECK(module != NULL);
    CHECK(declaration != NULL &&
          weivel_standard_read_library(declaration, length, &library, &refusal) == WEIVEL_STATUS_OK);
    weivel_standard_refusal_release(&refusal);
    CHECK(library.service_count == 1);
    if (module != NULL && library.service_count == 1) {
        check_text(library.services[0].id, WEIVEL_STANDARD_SERVICE_LOG, "the declared service");
        check_exported_service(module, &library.services[0], library.descriptions[0], "weivel_standard_log");
        check_library_log_keeps_nothing(module, &library.services[0]);
    }
    weivel_standard_library_release(&library);
    free(declaration);
    close_stub_host(module);
}

static void the_built_library_exports_what_its_declaration_names(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    size_t length;
    char *declaration = read_declaration(STUB_HOST_DECLARATION, &length);
    void *module = open_stub_host();
    CHECK(module != NULL);
    CHECK(declaration != NULL &&
          weivel_standard_read_library(declaration, length, &library, &refusal) == WEIVEL_STATUS_OK);
    if (refusal.detail != NULL) {
        fprintf(stderr, "the declaration was refused at %s: %s\n", refusal.path, refusal.detail);
    }
    weivel_standard_refusal_release(&refusal);
    CHECK(library.service_count == 2);
    if (module != NULL && library.service_count == 2) {
        check_text(library.services[0].id, WEIVEL_STANDARD_SERVICE_HOST, "the first service");
        check_text(library.services[1].id, WEIVEL_STANDARD_SERVICE_SCOPE, "the second service");
        check_exported_service(module, &library.services[0], library.descriptions[0], "weivel_standard_host");
        check_exported_service(module, &library.services[1], library.descriptions[1], "weivel_standard_scopes");
    }
    weivel_standard_library_release(&library);
    free(declaration);
    close_stub_host(module);
}

int main(void) {
    a_serve_run_answers_its_plan_in_order();
    an_action_run_answers_its_action();
    an_action_run_answers_the_service_method_it_names();
    one_of_service_and_method_is_answered_as_given();
    a_plan_from_an_older_consumer_is_written_only_within_its_size();
    a_method_run_refuses_a_plan_too_old_to_hold_it();
    a_run_file_that_breaks_its_shape_refuses_start();
    a_start_told_to_refuse_records_only_itself();
    start_refuses_a_call_that_is_not_well_made();
    started_records_each_outcome_in_order();
    started_told_to_refuse_refuses();
    tick_answers_each_listed_time_after_the_first_then_none();
    a_second_start_is_the_callers_mistake();
    a_start_after_a_stop_opens_a_new_run();
    a_start_after_a_refused_start_opens_a_run();
    a_tick_told_to_refuse_refuses();
    a_malformed_tick_answers_a_has_next_beyond_one();
    finished_records_a_result_none_a_failure_and_a_mistake();
    stop_answers_the_exit_code();
    scope_answers_a_scoped_plugin_its_own_definitions();
    scope_offers_an_unscoped_plugin_nothing_and_refuses_as_told();
    the_built_library_exports_what_its_declaration_names();
    the_log_declaration_names_a_log_the_library_exports();
    return check_summary("stub_host_test");
}
