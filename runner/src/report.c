#include "report.h"

#include <string.h>

static const verdict_status COLUMNS[VERDICT_STATUS_COUNT] = {
    VERDICT_PASSED, VERDICT_FAILED, VERDICT_ERRORED, VERDICT_AWAITING_LAYER};

static void write_heading(text_builder *out) {
    size_t at;
    text_append_format(out, "%-12s", "op");
    for (at = 0; at < VERDICT_STATUS_COUNT; at++) {
        text_append_format(out, "%22s", verdict_label(COLUMNS[at]));
    }
    text_append(out, "\n");
}

static size_t count_status(const verdict *verdicts, size_t count, const char *op, verdict_status status) {
    size_t at;
    size_t total = 0;
    for (at = 0; at < count; at++) {
        int in_row = op == NULL || strcmp(verdicts[at].vector->op, op) == 0;
        if (in_row && verdicts[at].status == status) {
            total++;
        }
    }
    return total;
}

static void write_row(text_builder *out, const char *label, const verdict *verdicts, size_t count, const char *op) {
    size_t at;
    text_append_format(out, "%-12s", label);
    for (at = 0; at < VERDICT_STATUS_COUNT; at++) {
        text_append_format(out, "%22lu", (unsigned long)count_status(verdicts, count, op, COLUMNS[at]));
    }
    text_append(out, "\n");
}

static void write_op_rows(text_builder *out, const verdict *verdicts, size_t count) {
    size_t op_count;
    const op_entry *ops = ops_all(&op_count);
    size_t at;
    for (at = 0; at < op_count; at++) {
        size_t total = 0;
        size_t status;
        for (status = 0; status < VERDICT_STATUS_COUNT; status++) {
            total += count_status(verdicts, count, ops[at].op, COLUMNS[status]);
        }
        if (total > 0) {
            write_row(out, ops[at].op, verdicts, count, ops[at].op);
        }
    }
}

static void write_reasons(text_builder *out, const char *reasons) {
    const char *line = reasons;
    while (*line != '\0') {
        size_t length = strcspn(line, "\n");
        text_append(out, "    ");
        text_append_bytes(out, line, length);
        text_append(out, "\n");
        line += length + (line[length] == '\n' ? 1 : 0);
    }
}

static void write_unjudged(text_builder *out, const verdict *verdicts, size_t count) {
    size_t at;
    int first = 1;
    for (at = 0; at < count; at++) {
        verdict_status status = verdicts[at].status;
        if (status != VERDICT_FAILED && status != VERDICT_ERRORED) {
            continue;
        }
        text_append(out, first ? "\n" : "");
        first = 0;
        text_append_format(out, "%s %s (covers %s)\n", status == VERDICT_FAILED ? "FAILED" : "ERRORED",
                           verdicts[at].vector->id, verdicts[at].vector->covers);
        write_reasons(out, verdicts[at].reasons);
    }
}

static void write_awaiting(text_builder *out, const verdict *verdicts, size_t count) {
    size_t layer = count_status(verdicts, count, NULL, VERDICT_AWAITING_LAYER);
    if (layer > 0) {
        text_append_format(out, "\n%lu layer vectors are awaiting a layer suite and have not been run\n",
                           (unsigned long)layer);
    }
}

int report_write(const verdict *verdicts, size_t count, text_builder *out) {
    write_heading(out);
    write_op_rows(out, verdicts, count);
    write_row(out, "total", verdicts, count, NULL);
    write_unjudged(out, verdicts, count);
    write_awaiting(out, verdicts, count);
    if (count == 0) {
        text_append(out, "\nno vectors ran, which is not a pass\n");
        return 1;
    }
    return count_status(verdicts, count, NULL, VERDICT_PASSED) == count ? 0 : 1;
}
