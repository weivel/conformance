#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <weivel/core.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#define EXIT_OVER_BUDGET 1
#define EXIT_UNUSABLE 2

#define WARMUP_CALLS 200000
#define CALLS_PER_BATCH 1000
#define BATCH_COUNT 2000

#define SERVICE_ID "bench:service"

typedef struct bench_core {
    weivel_core *core;
    weivel_handle plugin;
} bench_core;

typedef struct summary {
    double minimum;
    double lower_quartile;
    double median;
    double upper_quartile;
    double percentile_99;
    double maximum;
} summary;

static int now_nanoseconds(double *nanoseconds) {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter)) {
        return 0;
    }
    *nanoseconds = (double)counter.QuadPart * 1e9 / (double)frequency.QuadPart;
    return 1;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    *nanoseconds = (double)now.tv_sec * 1e9 + (double)now.tv_nsec;
    return 1;
#endif
}

static int build_core(bench_core *bench) {
    const char *consumed_id = SERVICE_ID;
    weivel_service_want want = WEIVEL_INIT(weivel_service_want);
    want.id = consumed_id;
    weivel_manifest manifest = WEIVEL_INIT(weivel_manifest);
    manifest.import_name = "bench";
    manifest.api = WEIVEL_API_VERSION;
    manifest.services.consumes.items = &want;
    manifest.services.consumes.count = 1;
    weivel_handle registration = 0;
    if (weivel_core_create(NULL, &bench->core, NULL) != WEIVEL_STATUS_OK) {
        return 0;
    }
    return weivel_register_manifest(bench->core, &manifest, &bench->plugin, NULL) == WEIVEL_STATUS_OK &&
           weivel_open_plugin(bench->core, bench->plugin, NULL, NULL) == WEIVEL_STATUS_OK &&
           weivel_register_service(bench->core, WEIVEL_HOST, SERVICE_ID, &registration, NULL) == WEIVEL_STATUS_OK;
}

static weivel_status call_once(const bench_core *bench, weivel_handle *implementation) {
    return weivel_get_service(bench->core, bench->plugin, SERVICE_ID, implementation, NULL);
}

static int warm_up(const bench_core *bench) {
    weivel_handle implementation = 0;
    for (int call = 0; call < WARMUP_CALLS; call++) {
        if (call_once(bench, &implementation) != WEIVEL_STATUS_OK || implementation == 0) {
            return 0;
        }
    }
    return 1;
}

static int time_batch(const bench_core *bench, double *nanoseconds_per_call) {
    weivel_handle implementation = 0;
    int any_refused_or_empty = 0;
    double start = 0;
    double stop = 0;
    if (!now_nanoseconds(&start)) {
        return 0;
    }
    for (int call = 0; call < CALLS_PER_BATCH; call++) {
        any_refused_or_empty |= call_once(bench, &implementation) != WEIVEL_STATUS_OK;
        any_refused_or_empty |= implementation == 0;
    }
    if (!now_nanoseconds(&stop)) {
        return 0;
    }
    *nanoseconds_per_call = (stop - start) / CALLS_PER_BATCH;
    return !any_refused_or_empty;
}

static int compare_doubles(const void *left, const void *right) {
    double difference = *(const double *)left - *(const double *)right;
    return (difference > 0) - (difference < 0);
}

static double percentile(const double *sorted, int count, double fraction) {
    int index = (int)ceil(fraction * count) - 1;
    return sorted[index < 0 ? 0 : index];
}

static summary summarise(double *samples, int count) {
    qsort(samples, (size_t)count, sizeof samples[0], compare_doubles);
    summary result = {samples[0],
                      percentile(samples, count, 0.25),
                      percentile(samples, count, 0.50),
                      percentile(samples, count, 0.75),
                      percentile(samples, count, 0.99),
                      samples[count - 1]};
    return result;
}

static int measure_into(summary *result) {
    bench_core bench = {NULL, 0};
    double *samples = malloc(sizeof(double) * BATCH_COUNT);
    int usable = samples != NULL && build_core(&bench) && warm_up(&bench);
    for (int batch = 0; usable && batch < BATCH_COUNT; batch++) {
        usable = time_batch(&bench, &samples[batch]);
    }
    if (usable) {
        *result = summarise(samples, BATCH_COUNT);
    }
    free(samples);
    weivel_core_destroy(bench.core);
    return usable && result->median > 0;
}

static void print_summary(const summary *result) {
    printf("calls_per_batch %d\n", CALLS_PER_BATCH);
    printf("batches %d\n", BATCH_COUNT);
    printf("warmup_calls %d\n", WARMUP_CALLS);
    printf("minimum_nanoseconds %.2f\n", result->minimum);
    printf("lower_quartile_nanoseconds %.2f\n", result->lower_quartile);
    printf("median_nanoseconds %.2f\n", result->median);
    printf("upper_quartile_nanoseconds %.2f\n", result->upper_quartile);
    printf("p99_nanoseconds %.2f\n", result->percentile_99);
    printf("maximum_nanoseconds %.2f\n", result->maximum);
    printf("spread_nanoseconds %.2f\n", result->upper_quartile - result->lower_quartile);
}

static int read_budget(const char *path, double *budget) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "budget file %s cannot be opened\n", path);
        return 0;
    }
    char key[64];
    char leftover[64];
    int fields = fscanf(file, "%63s %lf %63s", key, budget, leftover);
    fclose(file);
    if (fields != 2 || strcmp(key, "median_nanoseconds") != 0 || !isfinite(*budget) || !(*budget > 0)) {
        fprintf(stderr, "budget file %s must hold exactly: median_nanoseconds <positive number>\n", path);
        return 0;
    }
    return 1;
}

static int run_measure(void) {
    summary result;
    if (!measure_into(&result)) {
        fprintf(stderr, "the measurement read nothing\n");
        return EXIT_UNUSABLE;
    }
    print_summary(&result);
    return 0;
}

static int run_gate(const char *budget_path) {
    double budget = 0;
    summary result;
    if (!read_budget(budget_path, &budget)) {
        return EXIT_UNUSABLE;
    }
    if (!measure_into(&result)) {
        fprintf(stderr, "the measurement read nothing\n");
        return EXIT_UNUSABLE;
    }
    print_summary(&result);
    printf("budget_nanoseconds %.2f\n", budget);
    if (result.median > budget) {
        fprintf(stderr, "median %.2f ns is over the budget of %.2f ns\n", result.median, budget);
        return EXIT_OVER_BUDGET;
    }
    return 0;
}

int main(int argument_count, char **arguments) {
    if (argument_count == 2 && strcmp(arguments[1], "measure") == 0) {
        return run_measure();
    }
    if (argument_count == 3 && strcmp(arguments[1], "gate") == 0) {
        return run_gate(arguments[2]);
    }
    fprintf(stderr, "usage: weivel-benchmark-control-call measure | gate <budget-file>\n");
    return EXIT_UNUSABLE;
}
