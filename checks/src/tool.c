#define _POSIX_C_SOURCE 200809L

#include "tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/wait.h>
#endif

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} growing_text;

static int text_append(growing_text *text, const char *data, size_t length) {
    if (text->length + length + 1 > text->capacity) {
        size_t capacity = text->capacity == 0 ? 4096 : text->capacity;
        char *grown;
        while (text->length + length + 1 > capacity) {
            capacity *= 2;
        }
        grown = realloc(text->data, capacity);
        if (grown == NULL) {
            return 0;
        }
        text->data = grown;
        text->capacity = capacity;
    }
    memcpy(text->data + text->length, data, length);
    text->length += length;
    text->data[text->length] = '\0';
    return 1;
}

static int text_append_repeated(growing_text *text, char character, size_t times) {
    size_t at;
    for (at = 0; at < times; at++) {
        if (!text_append(text, &character, 1)) {
            return 0;
        }
    }
    return 1;
}

#ifdef _WIN32

/** Quotes one argument the way the C runtime's command-line parser reads it back, backslashes before a quote doubled. */
static int append_quoted_argument(growing_text *line, const char *argument) {
    size_t backslashes = 0;
    const char *cursor;
    int ok = text_append(line, "\"", 1);
    for (cursor = argument; ok && *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            backslashes++;
            continue;
        }
        if (*cursor == '"') {
            ok = text_append_repeated(line, '\\', backslashes * 2 + 1) && text_append(line, "\"", 1);
        } else {
            ok = text_append_repeated(line, '\\', backslashes) && text_append(line, cursor, 1);
        }
        backslashes = 0;
    }
    return ok && text_append_repeated(line, '\\', backslashes * 2) && text_append(line, "\"", 1);
}

static int build_command_line(const char *const *arguments, size_t count, growing_text *line) {
    size_t at;
    for (at = 0; at < count; at++) {
        if ((at > 0 && !text_append(line, " ", 1)) || !append_quoted_argument(line, arguments[at])) {
            return 0;
        }
    }
    return 1;
}

static int read_pipe(HANDLE pipe, growing_text *output) {
    char buffer[4096];
    DWORD read_count;
    while (ReadFile(pipe, buffer, sizeof buffer, &read_count, NULL) && read_count > 0) {
        if (!text_append(output, buffer, read_count)) {
            return 0;
        }
    }
    return text_append(output, "", 0);
}

int tool_run(const char *const *arguments, size_t count, char **output, int *exit_code, char *error,
    size_t error_size) {
    growing_text line = {NULL, 0, 0};
    growing_text captured = {NULL, 0, 0};
    SECURITY_ATTRIBUTES inheritable = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    HANDLE read_end;
    HANDLE write_end;
    DWORD code = 0;
    int read_ok;
    if (!build_command_line(arguments, count, &line)) {
        free(line.data);
        snprintf(error, error_size, "out of memory building the command line for %s", arguments[0]);
        return 0;
    }
    if (!CreatePipe(&read_end, &write_end, &inheritable, 0)) {
        free(line.data);
        snprintf(error, error_size, "cannot create a pipe to read %s", arguments[0]);
        return 0;
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = write_end;
    startup.hStdError = write_end;
    if (!CreateProcessA(NULL, line.data, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process)) {
        snprintf(error, error_size, "cannot run %s (error %lu)", arguments[0], (unsigned long)GetLastError());
        CloseHandle(read_end);
        CloseHandle(write_end);
        free(line.data);
        return 0;
    }
    free(line.data);
    CloseHandle(write_end);
    read_ok = read_pipe(read_end, &captured);
    CloseHandle(read_end);
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if (!read_ok) {
        free(captured.data);
        snprintf(error, error_size, "out of memory reading the output of %s", arguments[0]);
        return 0;
    }
    *output = captured.data;
    *exit_code = (int)code;
    return 1;
}

#else

/** Single quotes keep every character literal to the shell; a quote inside is closed, escaped and reopened. */
static int append_quoted_argument(growing_text *line, const char *argument) {
    const char *cursor;
    int ok = text_append(line, "'", 1);
    for (cursor = argument; ok && *cursor != '\0'; cursor++) {
        ok = *cursor == '\'' ? text_append(line, "'\\''", 4) : text_append(line, cursor, 1);
    }
    return ok && text_append(line, "'", 1);
}

static int build_command_line(const char *const *arguments, size_t count, growing_text *line) {
    size_t at;
    for (at = 0; at < count; at++) {
        if ((at > 0 && !text_append(line, " ", 1)) || !append_quoted_argument(line, arguments[at])) {
            return 0;
        }
    }
    return text_append(line, " 2>&1", 5);
}

static int read_stream(FILE *stream, growing_text *output) {
    char buffer[4096];
    size_t read_count;
    while ((read_count = fread(buffer, 1, sizeof buffer, stream)) > 0) {
        if (!text_append(output, buffer, read_count)) {
            return 0;
        }
    }
    return text_append(output, "", 0);
}

int tool_run(const char *const *arguments, size_t count, char **output, int *exit_code, char *error,
    size_t error_size) {
    growing_text line = {NULL, 0, 0};
    growing_text captured = {NULL, 0, 0};
    FILE *stream;
    int status;
    int read_ok;
    if (!build_command_line(arguments, count, &line)) {
        free(line.data);
        snprintf(error, error_size, "out of memory building the command line for %s", arguments[0]);
        return 0;
    }
    fflush(NULL);
    stream = popen(line.data, "r");
    free(line.data);
    if (stream == NULL) {
        snprintf(error, error_size, "cannot run %s", arguments[0]);
        return 0;
    }
    read_ok = read_stream(stream, &captured);
    status = pclose(stream);
    if (!read_ok) {
        free(captured.data);
        snprintf(error, error_size, "out of memory reading the output of %s", arguments[0]);
        return 0;
    }
    *output = captured.data;
    *exit_code = status != -1 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return 1;
}

#endif
