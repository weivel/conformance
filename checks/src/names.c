#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lockfile.h"
#include "tree.h"

static const char *const EXEMPT_PATHS[] = {"LICENSE", ".weivel/repo.json"};

#define BINARY_PROBE_BYTES 8192
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

typedef struct {
    size_t files_read;
    int offenders;
} scan_result;

static void lowercase(char *text, size_t length) {
    size_t at;
    for (at = 0; at < length; at++) {
        text[at] = (char)tolower((unsigned char)text[at]);
    }
}

static int is_exempt(const char *relative) {
    size_t at;
    for (at = 0; at < COUNT_OF(EXEMPT_PATHS); at++) {
        if (strcmp(relative, EXEMPT_PATHS[at]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_the_words_file(const char *root, const char *relative, const char *words_path) {
    char *full = tree_join(root, relative);
    char *words = tree_normalize_root(words_path);
    int same = full != NULL && words != NULL && strcmp(full, words) == 0;
    free(full);
    free(words);
    return same;
}

static int is_binary(const char *data, size_t size) {
    return memchr(data, '\0', size < BINARY_PROBE_BYTES ? size : BINARY_PROBE_BYTES) != NULL;
}

static void blank_nul_bytes(char *data, size_t size) {
    size_t at;
    for (at = 0; at < size; at++) {
        if (data[at] == '\0') {
            data[at] = ' ';
        }
    }
}

static size_t line_of(const char *data, const char *position) {
    size_t line = 1;
    const char *cursor;
    for (cursor = data; cursor < position; cursor++) {
        if (*cursor == '\n') {
            line++;
        }
    }
    return line;
}

static int report_words(const char *relative, char *data, size_t size, const text_list *words) {
    size_t at;
    int offenders = 0;
    blank_nul_bytes(data, size);
    lowercase(data, size);
    for (at = 0; at < words->count; at++) {
        const char *found = strstr(data, words->items[at]);
        if (found != NULL) {
            fprintf(stderr, "%s:%zu: names '%s'\n", relative, line_of(data, found), words->items[at]);
            offenders++;
        }
    }
    return offenders;
}

static int is_blank(char character) {
    return character == ' ' || character == '\t' || character == '\r';
}

static int line_is_word(const char *start, size_t length, const char *word) {
    size_t at;
    if (length != strlen(word)) {
        return 0;
    }
    for (at = 0; at < length; at++) {
        if (tolower((unsigned char)start[at]) != (unsigned char)word[at]) {
            return 0;
        }
    }
    return 1;
}

/**
 * Whether the text's trimmed, non-empty lines are the word list, word for word and in order, whatever its line
 * endings.
 *
 * @implNote The kit is checked with the list from its pinned commit, which sits outside the tree, so the kit's own
 * copy of the list has to be recognised by what it says rather than where it lies.
 */
static int is_a_copy_of_the_words(const char *data, size_t size, const text_list *words) {
    size_t matched = 0;
    size_t at = 0;
    while (at < size) {
        size_t end = at;
        size_t start = at;
        while (end < size && data[end] != '\n') {
            end++;
        }
        at = end + 1;
        while (start < end && is_blank(data[start])) {
            start++;
        }
        while (end > start && is_blank(data[end - 1])) {
            end--;
        }
        if (end == start) {
            continue;
        }
        if (matched == words->count || !line_is_word(data + start, end - start, words->items[matched])) {
            return 0;
        }
        matched++;
    }
    return matched == words->count;
}

static void scan_file(const char *root, const char *relative, const char *words_path, const text_list *words,
                      scan_result *result) {
    char *full;
    char *data;
    size_t size;
    if (is_exempt(relative) || is_the_words_file(root, relative, words_path)) {
        return;
    }
    full = tree_join(root, relative);
    if (full == NULL || !tree_read_file(full, &data, &size)) {
        fprintf(stderr, "%s: cannot be read\n", relative);
        result->offenders++;
        free(full);
        return;
    }
    free(full);
    if (lockfile_is_lockfile(relative)) {
        result->files_read++;
        result->offenders += lockfile_report_words(relative, data, size, words);
    } else if (is_binary(data, size)) {
        fprintf(stderr, "%s: skipped as binary\n", relative);
    } else if (is_a_copy_of_the_words(data, size, words)) {
        fprintf(stderr, "%s: skipped as a copy of the word list\n", relative);
    } else {
        result->files_read++;
        result->offenders += report_words(relative, data, size, words);
    }
    free(data);
}

static void lowercase_words(text_list *words) {
    size_t at;
    for (at = 0; at < words->count; at++) {
        lowercase(words->items[at], strlen(words->items[at]));
    }
}

static int load_words(const char *path, text_list *words) {
    char error[512];
    if (!tree_read_lines(path, words, error, sizeof error)) {
        fprintf(stderr, "weivel-names: %s\n", error);
        return 0;
    }
    if (words->count == 0) {
        fprintf(stderr, "weivel-names: no words in %s, so nothing was checked\n", path);
        text_list_free(words);
        return 0;
    }
    lowercase_words(words);
    return 1;
}

int main(int argument_count, char **arguments) {
    char error[512];
    text_list words;
    text_list files;
    scan_result result = {0, 0};
    char *root;
    size_t at;
    if (argument_count != 3) {
        fprintf(stderr, "usage: weivel-names <root> <words-file>\n");
        return 2;
    }
    if (!load_words(arguments[2], &words)) {
        return 2;
    }
    root = tree_normalize_root(arguments[1]);
    if (root == NULL || !tree_list_files(root, &files, error, sizeof error)) {
        fprintf(stderr, "weivel-names: %s\n", root == NULL ? "out of memory" : error);
        text_list_free(&words);
        free(root);
        return 2;
    }
    for (at = 0; at < files.count; at++) {
        scan_file(root, files.items[at], arguments[2], &words, &result);
    }
    if (result.files_read == 0) {
        fprintf(stderr, "weivel-names: no text files read under %s, so nothing was checked\n", root);
    } else if (result.offenders == 0) {
        printf("weivel-names: %zu files carry none of %zu names\n", result.files_read, words.count);
    }
    text_list_free(&files);
    text_list_free(&words);
    free(root);
    return result.files_read == 0 ? 2 : result.offenders > 0 ? 1 : 0;
}
