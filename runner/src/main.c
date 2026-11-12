#include "report.h"
#include "runner.h"
#include "vectors.h"

#include <stdio.h>
#include <string.h>

#define ERROR_CAPACITY 1024

static int usage(void) {
    fprintf(stderr, "usage: weivel-conformance run <directory>\n");
    return 2;
}

static int run_directory(const char *directory) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    verdict *verdicts;
    text_builder report;
    int exit_code;
    if (!corpus_load(directory, &loaded, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 2;
    }
    verdicts = run_corpus(&loaded);
    if (verdicts == NULL) {
        fprintf(stderr, "out of memory\n");
        corpus_free(&loaded);
        return 2;
    }
    text_init(&report);
    exit_code = report_write(verdicts, loaded.count, &report);
    fputs(text_view(&report), stdout);
    text_release(&report);
    verdicts_free(verdicts, loaded.count);
    corpus_free(&loaded);
    return exit_code;
}

int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "run") != 0) {
        return usage();
    }
    return run_directory(argv[2]);
}
