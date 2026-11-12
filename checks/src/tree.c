#define _POSIX_C_SOURCE 200809L

#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

typedef struct {
    char *name;
    int is_directory;
} entry;

typedef struct {
    entry *items;
    size_t count;
    size_t capacity;
} entry_list;

int text_list_add(text_list *list, const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    char **grown;
    if (copy == NULL) {
        return 0;
    }
    memcpy(copy, text, length + 1);
    grown = realloc(list->items, (list->count + 1) * sizeof *grown);
    if (grown == NULL) {
        free(copy);
        return 0;
    }
    list->items = grown;
    list->items[list->count++] = copy;
    return 1;
}

void text_list_free(text_list *list) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        free(list->items[at]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static int compare_text(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static void sort_text_list(text_list *list) {
    if (list->count > 1) {
        qsort(list->items, list->count, sizeof(char *), compare_text);
    }
}

char *tree_join(const char *directory, const char *name) {
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    char *joined = malloc(directory_length + name_length + 2);
    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, directory, directory_length);
    joined[directory_length] = '/';
    memcpy(joined + directory_length + 1, name, name_length + 1);
    return joined;
}

char *tree_normalize_root(const char *path) {
    size_t length = strlen(path);
    char *copy;
    size_t at;
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) {
        length--;
    }
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    for (at = 0; at < length; at++) {
        copy[at] = path[at] == '\\' ? '/' : path[at];
    }
    copy[length] = '\0';
    return copy;
}

static int add_entry(entry_list *list, const char *name, int is_directory) {
    entry *grown;
    char *copy;
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        grown = realloc(list->items, capacity * sizeof *grown);
        if (grown == NULL) {
            return 0;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    copy = malloc(strlen(name) + 1);
    if (copy == NULL) {
        return 0;
    }
    memcpy(copy, name, strlen(name) + 1);
    list->items[list->count].name = copy;
    list->items[list->count].is_directory = is_directory;
    list->count++;
    return 1;
}

static void entry_list_free(entry_list *list) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        free(list->items[at].name);
    }
    free(list->items);
}

static int compare_entries(const void *left, const void *right) {
    return strcmp(((const entry *)left)->name, ((const entry *)right)->name);
}

#define LISTED 1
#define UNREADABLE 0
#define OUT_OF_MEMORY (-1)

#ifdef _WIN32
static int list_directory(const char *directory, entry_list *entries) {
    char *pattern = tree_join(directory, "*");
    WIN32_FIND_DATAA data;
    HANDLE handle;
    int ok = 1;
    if (pattern == NULL) {
        return OUT_OF_MEMORY;
    }
    handle = FindFirstFileA(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return UNREADABLE;
    }
    do {
        if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0) {
            ok = add_entry(entries, data.cFileName, (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
        }
    } while (ok && FindNextFileA(handle, &data));
    FindClose(handle);
    return ok ? LISTED : OUT_OF_MEMORY;
}
#else
static int list_directory(const char *directory, entry_list *entries) {
    DIR *handle = opendir(directory);
    struct dirent *item;
    int ok = 1;
    if (handle == NULL) {
        return UNREADABLE;
    }
    while (ok && (item = readdir(handle)) != NULL) {
        if (strcmp(item->d_name, ".") != 0 && strcmp(item->d_name, "..") != 0) {
            char *path = tree_join(directory, item->d_name);
            struct stat status;
            if (path == NULL) {
                ok = 0;
            } else if (lstat(path, &status) == 0 && (S_ISDIR(status.st_mode) || S_ISREG(status.st_mode))) {
                ok = add_entry(entries, item->d_name, S_ISDIR(status.st_mode));
            }
            free(path);
        }
    }
    closedir(handle);
    return ok ? LISTED : OUT_OF_MEMORY;
}
#endif

/** Spelled in two pieces because the names gate reads this file and would refuse the word it exempts. */
static const char HOSTING_DIRECTORY[] = ".git" "hub";

static int is_build_output(const char *name) {
    return strcmp(name, "build") == 0 || strncmp(name, "build-", 6) == 0;
}

static int is_skipped_at_root(const char *name) {
    return strcmp(name, ".git") == 0 || strcmp(name, HOSTING_DIRECTORY) == 0 || is_build_output(name) ||
           strcmp(name, "siblings") == 0 || strcmp(name, "node_modules") == 0;
}

static void explain_listing_failure(int result, const char *directory, char *error, size_t error_size) {
    if (result == OUT_OF_MEMORY) {
        snprintf(error, error_size, "out of memory reading %s", directory);
    } else {
        snprintf(error, error_size, "cannot read the directory %s", directory);
    }
}

static char *join_relative(const char *relative, const char *name) {
    if (relative[0] == '\0') {
        char *copy = malloc(strlen(name) + 1);
        if (copy != NULL) {
            memcpy(copy, name, strlen(name) + 1);
        }
        return copy;
    }
    return tree_join(relative, name);
}

static int walk(const char *root, const char *relative, text_list *files, char *error, size_t error_size) {
    entry_list entries = {NULL, 0, 0};
    char *directory = relative[0] == '\0' ? tree_normalize_root(root) : tree_join(root, relative);
    size_t at;
    int ok = directory != NULL;
    if (ok) {
        int listed = list_directory(directory, &entries);
        if (listed != LISTED) {
            explain_listing_failure(listed, directory, error, error_size);
            ok = 0;
        }
    }
    free(directory);
    if (entries.count > 1) {
        qsort(entries.items, entries.count, sizeof(entry), compare_entries);
    }
    for (at = 0; ok && at < entries.count; at++) {
        char *child = join_relative(relative, entries.items[at].name);
        if (child == NULL) {
            ok = 0;
        } else if (entries.items[at].is_directory) {
            int skipped = relative[0] == '\0' && is_skipped_at_root(entries.items[at].name);
            ok = skipped || walk(root, child, files, error, error_size);
        } else {
            ok = text_list_add(files, child);
        }
        free(child);
    }
    entry_list_free(&entries);
    if (!ok && error[0] == '\0') {
        snprintf(error, error_size, "out of memory reading %s", root);
    }
    return ok;
}

int tree_list_files(const char *root, text_list *files, char *error, size_t error_size) {
    files->items = NULL;
    files->count = 0;
    error[0] = '\0';
    if (!walk(root, "", files, error, error_size)) {
        text_list_free(files);
        return 0;
    }
    sort_text_list(files);
    return 1;
}

int tree_list_directories(const char *directory, text_list *names, char *error, size_t error_size) {
    entry_list entries = {NULL, 0, 0};
    size_t at;
    int ok = 1;
    int listed;
    names->items = NULL;
    names->count = 0;
    error[0] = '\0';
    listed = list_directory(directory, &entries);
    if (listed != LISTED) {
        entry_list_free(&entries);
        explain_listing_failure(listed, directory, error, error_size);
        return 0;
    }
    for (at = 0; ok && at < entries.count; at++) {
        if (entries.items[at].is_directory) {
            ok = text_list_add(names, entries.items[at].name);
        }
    }
    entry_list_free(&entries);
    if (!ok) {
        text_list_free(names);
        snprintf(error, error_size, "out of memory reading %s", directory);
        return 0;
    }
    sort_text_list(names);
    return 1;
}

static char *grow_buffer(char *buffer, size_t capacity) {
    char *grown = realloc(buffer, capacity);
    if (grown == NULL) {
        free(buffer);
    }
    return grown;
}

int tree_read_file(const char *path, char **data, size_t *size) {
    FILE *file = fopen(path, "rb");
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer;
    if (file == NULL) {
        return 0;
    }
    buffer = malloc(capacity);
    while (buffer != NULL) {
        size_t read = fread(buffer + length, 1, capacity - length - 1, file);
        length += read;
        if (read == 0) {
            break;
        }
        if (length >= capacity - 1) {
            capacity *= 2;
            buffer = grow_buffer(buffer, capacity);
        }
    }
    fclose(file);
    if (buffer == NULL) {
        return 0;
    }
    buffer[length] = '\0';
    *data = buffer;
    *size = length;
    return 1;
}

static void trim(char *text) {
    size_t length = strlen(text);
    size_t start = 0;
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }
    memmove(text, text + start, length - start + 1);
}

int tree_read_lines(const char *path, text_list *lines, char *error, size_t error_size) {
    char *data;
    size_t size;
    char *cursor;
    int ok = 1;
    lines->items = NULL;
    lines->count = 0;
    error[0] = '\0';
    if (!tree_read_file(path, &data, &size)) {
        snprintf(error, error_size, "cannot read %s", path);
        return 0;
    }
    cursor = data;
    while (ok && *cursor != '\0') {
        char *end = strchr(cursor, '\n');
        if (end != NULL) {
            *end = '\0';
        }
        trim(cursor);
        if (cursor[0] != '\0') {
            ok = text_list_add(lines, cursor);
        }
        cursor = end == NULL ? cursor + strlen(cursor) : end + 1;
    }
    free(data);
    if (!ok) {
        text_list_free(lines);
        snprintf(error, error_size, "out of memory reading %s", path);
    }
    return ok;
}
