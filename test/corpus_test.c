#include "check.h"
#include "runner.h"
#include "vectors.h"

#include <stdlib.h>

#define ERROR_CAPACITY 1024

typedef struct {
    const char *directory;
    size_t count;
} pinned_count;

static const pinned_count PINNED[] = {
    {"core/supports", 4},   {"core/boot", 3},          {"core/order", 6},
    {"core/resolve", 6},    {"core/revoke", 2},        {"core/scope", 2},
    {"core/want", 3},       {"core/reach", 16},        {"core/unbound", 3},
    {"core/carry", 1},      {"core/consume", 16},      {"standard/identity", 14},
    {"standard/validate", 30}, {"layer/consume", 2},    {"layer/want", 4},
    {"layer/lifecycle", 3},
};

static const char *const SECTIONS[] = {"12.1",         "12.2",         "12.3",         "12.4",
                                       "12.5",         "12.6",         "invariant-11", "invariant-12",
                                       "invariant-14", "invariant-4",  "invariant-8"};

static int load_fixture(const char *name, corpus *loaded, char *error) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", KIT_FIXTURES_DIR, name);
    return corpus_load(path, loaded, error, ERROR_CAPACITY);
}

static void check_refused(const char *fixture, const char *needle) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    int ok = load_fixture(fixture, &loaded, error);
    CHECK(!ok);
    if (ok) {
        corpus_free(&loaded);
        return;
    }
    if (strstr(error, needle) == NULL) {
        fprintf(stderr, "%s: expected an error mentioning \"%s\", got \"%s\"\n", fixture, needle, error);
        check_failures++;
    }
}

static void pins_the_count_of_every_directory(void) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    size_t total = 0;
    size_t at;
    int ok = corpus_load(KIT_VECTORS_DIR, &loaded, error, sizeof error);
    if (!ok) {
        fprintf(stderr, "the corpus did not load: %s\n", error);
    }
    CHECK(ok);
    if (!ok) {
        return;
    }
    for (at = 0; at < sizeof PINNED / sizeof PINNED[0]; at++) {
        size_t found = corpus_count_in(&loaded, PINNED[at].directory);
        if (found != PINNED[at].count) {
            fprintf(stderr, "%s: expected %lu vectors, found %lu\n", PINNED[at].directory,
                    (unsigned long)PINNED[at].count, (unsigned long)found);
            check_failures++;
        }
        total += PINNED[at].count;
    }
    CHECK(loaded.count == total);
    corpus_free(&loaded);
}

static int has_text(const char *const *list, size_t count, const char *wanted) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (strcmp(list[at], wanted) == 0) {
            return 1;
        }
    }
    return 0;
}

static void covers_every_section_the_kit_pins(void) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    const char *seen[200];
    size_t seen_count = 0;
    size_t at;
    CHECK(corpus_load(KIT_VECTORS_DIR, &loaded, error, sizeof error));
    for (at = 0; at < loaded.count; at++) {
        if (!has_text(seen, seen_count, loaded.items[at].covers)) {
            seen[seen_count++] = loaded.items[at].covers;
        }
    }
    CHECK(seen_count == sizeof SECTIONS / sizeof SECTIONS[0]);
    for (at = 0; at < sizeof SECTIONS / sizeof SECTIONS[0]; at++) {
        if (!has_text(seen, seen_count, SECTIONS[at])) {
            fprintf(stderr, "no vector covers %s\n", SECTIONS[at]);
            check_failures++;
        }
    }
    corpus_free(&loaded);
}

static void asks_about_every_op_the_kit_knows(void) {
    static const char *const OPS[] = {"supports", "boot",    "order",    "resolve", "revoke",
                                      "scope",    "want",    "reach",    "unbound", "carry",
                                      "consume",  "identity", "validate", "lifecycle"};
    corpus loaded;
    char error[ERROR_CAPACITY];
    size_t at;
    size_t other;
    CHECK(corpus_load(KIT_VECTORS_DIR, &loaded, error, sizeof error));
    for (at = 0; at < sizeof OPS / sizeof OPS[0]; at++) {
        int asked = 0;
        for (other = 0; other < loaded.count; other++) {
            asked = asked || strcmp(loaded.items[other].op, OPS[at]) == 0;
        }
        if (!asked) {
            fprintf(stderr, "no vector asks about %s\n", OPS[at]);
            check_failures++;
        }
    }
    corpus_free(&loaded);
}

static void refuses_an_unknown_op(void) {
    check_refused("unknown-op", "unknown op 'reticulate'");
}

static void refuses_a_duplicate_id(void) {
    check_refused("duplicate-ids", "duplicate vector id supports/same-id");
}

static void refuses_an_id_outside_its_op_family(void) {
    check_refused("wrong-family", "runs op supports, so its id must start supports/");
}

static void refuses_a_vector_that_asserts_nothing(void) {
    check_refused("asserts-nothing", "then asserts nothing");
}

static void refuses_a_file_that_is_not_json(void) {
    check_refused("malformed", "is not readable JSON");
}

static void refuses_a_section_the_design_does_not_state(void) {
    check_refused("bad-covers", "covers must be");
}

static void refuses_a_directory_without_vectors(void) {
    check_refused("no-vectors", "no vectors under");
}

static void refuses_a_directory_that_does_not_exist(void) {
    check_refused("absent-directory", "cannot read the directory");
}

static void refuses_a_vector_outside_every_tier_directory(void) {
    check_refused("no-tier", "no suite owns it");
}

static void the_last_tier_directory_in_a_path_decides(void) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    CHECK(load_fixture("nested-tiers", &loaded, error));
    CHECK(loaded.count == 1);
    CHECK(loaded.items[0].tier == TIER_STANDARD);
    check_text(loaded.items[0].directory, "standard/identity", "nested directory");
    corpus_free(&loaded);
}

static void refuses_present_false(void) {
    check_refused("present-false", "present: false asserts nothing");
}

static void refuses_absent_false(void) {
    check_refused("absent-false", "absent: false asserts nothing");
}

static void judges_every_vector_of_a_root_below_its_tier(const char *subdirectory, vector_tier tier,
                                                          verdict_status expected) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    char path[512];
    verdict *verdicts;
    size_t at;
    snprintf(path, sizeof path, "%s/%s", KIT_VECTORS_DIR, subdirectory);
    CHECK(corpus_load(path, &loaded, error, sizeof error));
    verdicts = run_corpus(&loaded);
    CHECK(verdicts != NULL);
    CHECK(loaded.count > 0);
    for (at = 0; verdicts != NULL && at < loaded.count; at++) {
        CHECK(loaded.items[at].tier == tier);
        CHECK(verdicts[at].status == expected);
    }
    verdicts_free(verdicts, loaded.count);
    corpus_free(&loaded);
}

static void a_root_inside_a_tier_is_still_that_tier(void) {
    judges_every_vector_of_a_root_below_its_tier("standard/identity", TIER_STANDARD, VERDICT_PASSED);
    judges_every_vector_of_a_root_below_its_tier("layer/consume", TIER_LAYER, VERDICT_AWAITING_LAYER);
    judges_every_vector_of_a_root_below_its_tier("layer", TIER_LAYER, VERDICT_AWAITING_LAYER);
    judges_every_vector_of_a_root_below_its_tier("standard", TIER_STANDARD, VERDICT_PASSED);
}

static size_t passed_in(const corpus *loaded, const verdict *verdicts, const char *directory) {
    size_t at;
    size_t passed = 0;
    for (at = 0; at < loaded->count; at++) {
        if (strcmp(loaded->items[at].directory, directory) == 0 && verdicts[at].status == VERDICT_PASSED) {
            passed++;
        }
    }
    return passed;
}

static void runs_every_pinned_standard_vector_and_passes_it(void) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    verdict *verdicts;
    size_t at;
    CHECK(corpus_load(KIT_VECTORS_DIR, &loaded, error, sizeof error));
    verdicts = run_corpus(&loaded);
    CHECK(verdicts != NULL);
    for (at = 0; verdicts != NULL && at < sizeof PINNED / sizeof PINNED[0]; at++) {
        size_t passed;
        if (strncmp(PINNED[at].directory, "standard/", 9) != 0) {
            continue;
        }
        passed = passed_in(&loaded, verdicts, PINNED[at].directory);
        if (passed != PINNED[at].count) {
            fprintf(stderr, "%s: expected %lu vectors run and passed, found %lu\n", PINNED[at].directory,
                    (unsigned long)PINNED[at].count, (unsigned long)passed);
            check_failures++;
        }
    }
    verdicts_free(verdicts, loaded.count);
    corpus_free(&loaded);
}

static void admits_every_invariant_the_model_states_and_no_other(void) {
    int invariant;
    char covers[32];
    for (invariant = 1; invariant <= 14; invariant++) {
        snprintf(covers, sizeof covers, "invariant-%d", invariant);
        CHECK(covers_is_valid(covers));
    }
    CHECK(!covers_is_valid("invariant-15"));
    CHECK(!covers_is_valid("invariant-0"));
    CHECK(!covers_is_valid("invariant-01"));
    CHECK(!covers_is_valid("invariant-"));
    CHECK(covers_is_valid("12.1"));
    CHECK(covers_is_valid("12.6"));
    CHECK(!covers_is_valid("12.7"));
    CHECK(!covers_is_valid("12.10"));
}

static void reads_the_tier_from_the_directory(void) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    size_t at;
    CHECK(load_fixture("tiers", &loaded, error));
    CHECK(loaded.count == 3);
    for (at = 0; at < loaded.count; at++) {
        const vector *candidate = &loaded.items[at];
        if (strcmp(candidate->id, "boot/three") == 0) {
            CHECK(candidate->tier == TIER_CORE);
            check_text(candidate->directory, "core/boot", "core directory");
        } else if (strcmp(candidate->id, "consume/two") == 0) {
            CHECK(candidate->tier == TIER_LAYER);
            check_text(candidate->directory, "layer/consume", "layer directory");
        } else {
            CHECK(candidate->tier == TIER_STANDARD);
            check_text(candidate->directory, "standard/identity", "standard directory");
        }
    }
    corpus_free(&loaded);
}

static void reads_the_tier_from_the_walked_root_when_it_is_a_tier_name(void) {
    corpus loaded;
    char error[ERROR_CAPACITY];
    char path[512];
    snprintf(path, sizeof path, "%s/tiers/layer", KIT_FIXTURES_DIR);
    CHECK(corpus_load(path, &loaded, error, sizeof error));
    CHECK(loaded.count == 1);
    CHECK(loaded.items[0].tier == TIER_LAYER);
    check_text(loaded.items[0].directory, "layer/consume", "directory under a tier root");
    corpus_free(&loaded);
}

int main(void) {
    pins_the_count_of_every_directory();
    covers_every_section_the_kit_pins();
    asks_about_every_op_the_kit_knows();
    refuses_an_unknown_op();
    refuses_a_duplicate_id();
    refuses_an_id_outside_its_op_family();
    refuses_a_vector_that_asserts_nothing();
    refuses_a_file_that_is_not_json();
    refuses_a_section_the_design_does_not_state();
    refuses_a_directory_without_vectors();
    refuses_a_directory_that_does_not_exist();
    refuses_a_vector_outside_every_tier_directory();
    the_last_tier_directory_in_a_path_decides();
    refuses_present_false();
    refuses_absent_false();
    a_root_inside_a_tier_is_still_that_tier();
    runs_every_pinned_standard_vector_and_passes_it();
    admits_every_invariant_the_model_states_and_no_other();
    reads_the_tier_from_the_directory();
    reads_the_tier_from_the_walked_root_when_it_is_a_tier_name();
    return check_summary("corpus_test");
}
