#define _POSIX_C_SOURCE 200809L

#include "header_tokens.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tool.h"
#include "tree.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#define SCRATCH_PATH_CAPACITY 1024

static int is_identifier_start(char character) {
    return isalpha((unsigned char)character) || character == '_';
}

static int is_identifier_character(char character) {
    return isalnum((unsigned char)character) || character == '_';
}

static int starts_an_include(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (*line != '#') {
        return 0;
    }
    line++;
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return strncmp(line, "include", strlen("include")) == 0;
}

/** Blanks every `#include` line in place, keeping its newline so the header's lines still count. */
static void blank_includes(char *text) {
    char *line = text;
    while (*line != '\0') {
        char *end = strchr(line, '\n');
        char *stop = end != NULL ? end : line + strlen(line);
        if (starts_an_include(line)) {
            memset(line, ' ', (size_t)(stop - line));
        }
        line = end != NULL ? end + 1 : stop;
    }
}

#ifdef _WIN32
static int make_scratch_path(char *path) {
    char directory[MAX_PATH];
    DWORD length = GetTempPathA((DWORD)sizeof directory, directory);
    return length > 0 && length < sizeof directory && GetTempFileNameA(directory, "wvh", 0, path) != 0;
}
#else
static int make_scratch_path(char *path) {
    const char *directory = getenv("TMPDIR");
    int descriptor;
    snprintf(path, SCRATCH_PATH_CAPACITY, "%s/weivel-header-XXXXXX",
             directory != NULL && directory[0] != '\0' ? directory : "/tmp");
    descriptor = mkstemp(path);
    if (descriptor < 0) {
        return 0;
    }
    close(descriptor);
    return 1;
}
#endif

static FILE *open_for_writing(const char *path) {
#ifdef _MSC_VER
    FILE *file = NULL;
    return fopen_s(&file, path, "wb") == 0 ? file : NULL;
#else
    return fopen(path, "wb");
#endif
}

static int write_scratch(const char *text, char *path) {
    FILE *file;
    size_t length = strlen(text);
    int written;
    if (!make_scratch_path(path)) {
        return 0;
    }
    file = open_for_writing(path);
    if (file == NULL) {
        remove(path);
        return 0;
    }
    written = fwrite(text, 1, length, file) == length;
    written = fclose(file) == 0 && written;
    if (!written) {
        remove(path);
    }
    return written;
}

static const char *base_name(const char *path, size_t length, size_t *base_length) {
    size_t at = length;
    while (at > 0 && path[at - 1] != '/' && path[at - 1] != '\\') {
        at--;
    }
    *base_length = length - at;
    return path + at;
}

/** Reads a line marker (`# 12 "file"` or `#line 12 "file"`): answers 1 with its line and whether it names `own`. */
static int read_marker(const char *line, const char *own, size_t *number, int *is_own) {
    const char *cursor = line + 1;
    const char *name;
    const char *name_end;
    size_t base_length;
    size_t own_length;
    const char *base;
    const char *own_base = base_name(own, strlen(own), &own_length);
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (strncmp(cursor, "line", 4) == 0 && !is_identifier_character(cursor[4])) {
        cursor += 4;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
    }
    if (!isdigit((unsigned char)*cursor)) {
        return 0;
    }
    *number = (size_t)strtoul(cursor, NULL, 10);
    while (isdigit((unsigned char)*cursor) || *cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '"') {
        return 0;
    }
    name = cursor + 1;
    name_end = strchr(name, '"');
    if (name_end == NULL) {
        return 0;
    }
    base = base_name(name, (size_t)(name_end - name), &base_length);
    *is_own = base_length == own_length && strncmp(base, own_base, own_length) == 0;
    return 1;
}

static int add_token(header_tokens *tokens, const char *text, size_t length, size_t line) {
    header_token *grown = realloc(tokens->items, (tokens->count + 1) * sizeof *grown);
    if (grown == NULL) {
        return 0;
    }
    tokens->items = grown;
    grown[tokens->count].text = text;
    grown[tokens->count].length = length;
    grown[tokens->count].line = line;
    tokens->count++;
    return 1;
}

static const char *skip_literal(const char *cursor, const char *end) {
    char quote = *cursor++;
    while (cursor < end && *cursor != quote) {
        cursor += *cursor == '\\' && cursor + 1 < end ? 2 : 1;
    }
    return cursor < end ? cursor + 1 : end;
}

static const char *skip_number(const char *cursor, const char *end) {
    while (cursor < end) {
        char previous = *cursor++;
        if (cursor < end && (*cursor == '+' || *cursor == '-') && strchr("eEpP", previous) != NULL) {
            cursor++;
        } else if (cursor < end && !is_identifier_character(*cursor) && *cursor != '.') {
            break;
        }
    }
    return cursor;
}

static int tokenize_line(header_tokens *tokens, const char *cursor, const char *end, size_t line) {
    while (cursor < end) {
        const char *start = cursor;
        if (isspace((unsigned char)*cursor)) {
            cursor++;
            continue;
        }
        if (*cursor == '"' || *cursor == '\'') {
            cursor = skip_literal(cursor, end);
            continue;
        }
        if (is_identifier_start(*cursor)) {
            while (cursor < end && is_identifier_character(*cursor)) {
                cursor++;
            }
        } else if (isdigit((unsigned char)*cursor) || (*cursor == '.' && cursor + 1 < end && isdigit((unsigned char)cursor[1]))) {
            cursor = skip_number(cursor, end);
        } else {
            cursor++;
        }
        if (!add_token(tokens, start, (size_t)(cursor - start), line)) {
            return 0;
        }
    }
    return 1;
}

/** Tokenizes the lines of the preprocessed `output` that its line markers attribute to `own`. */
static int tokenize_own_lines(header_tokens *tokens, const char *own) {
    const char *line = tokens->preprocessed;
    size_t number = 1;
    int is_own = 0;
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        const char *stop = end != NULL ? end : line + strlen(line);
        const char *first = line;
        while (first < stop && (*first == ' ' || *first == '\t')) {
            first++;
        }
        if (*first == '#' && read_marker(first, own, &number, &is_own)) {
            line = end != NULL ? end + 1 : stop;
            continue;
        }
        if (is_own && *first != '#' && !tokenize_line(tokens, line, stop, number)) {
            return 0;
        }
        number++;
        line = end != NULL ? end + 1 : stop;
    }
    return 1;
}

static int preprocess(const char *path, const char *scratch, header_tokens *tokens, char *error, size_t error_size) {
#if CORE_GATE_MSVC_STYLE
    const char *const arguments[] = {CORE_GATE_C_COMPILER, "/nologo", "/E", "/TC", scratch};
#else
    const char *const arguments[] = {CORE_GATE_C_COMPILER, "-E", "-x", "c", scratch};
#endif
    int exit_code;
    if (!tool_run(arguments, sizeof arguments / sizeof arguments[0], &tokens->preprocessed, &exit_code, error,
                  error_size)) {
        return 0;
    }
    if (exit_code != 0) {
        snprintf(error, error_size, "the preprocessor %s refused %s, exiting %d:\n%s", CORE_GATE_C_COMPILER, path,
                 exit_code, tokens->preprocessed);
        return 0;
    }
    return 1;
}

int header_tokens_read(const char *path, header_tokens *tokens, char *error, size_t error_size) {
    char scratch[SCRATCH_PATH_CAPACITY];
    char *text = NULL;
    size_t size;
    int read;
    memset(tokens, 0, sizeof *tokens);
    if (!tree_read_file(path, &text, &size)) {
        snprintf(error, error_size, "cannot read %s", path);
        return 0;
    }
    blank_includes(text);
    if (!write_scratch(text, scratch)) {
        free(text);
        snprintf(error, error_size, "cannot write a scratch copy of %s to preprocess", path);
        return 0;
    }
    free(text);
    read = preprocess(path, scratch, tokens, error, error_size);
    remove(scratch);
    if (read && !tokenize_own_lines(tokens, scratch)) {
        snprintf(error, error_size, "out of memory");
        read = 0;
    }
    if (!read) {
        header_tokens_free(tokens);
    }
    return read;
}

void header_tokens_free(header_tokens *tokens) {
    free(tokens->items);
    free(tokens->preprocessed);
    memset(tokens, 0, sizeof *tokens);
}
