#define _POSIX_C_SOURCE 200809L

#include "walk.h"

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

static void fail(char *error, size_t error_size, const char *what, const char *path) {
    snprintf(error, error_size, "%s %s", what, path);
}

static int add_path(path_list *list, char *path) {
    char **grown = realloc(list->paths, (list->count + 1) * sizeof *grown);
    if (grown == NULL) {
        free(path);
        return 0;
    }
    list->paths = grown;
    list->paths[list->count++] = path;
    return 1;
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

static char *join(const char *directory, const char *name) {
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

#ifdef _WIN32
static int list_directory(const char *directory, entry_list *entries) {
    char *pattern = join(directory, "*");
    WIN32_FIND_DATAA data;
    HANDLE handle;
    int ok = 1;
    if (pattern == NULL) {
        return 0;
    }
    handle = FindFirstFileA(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0) {
            ok = add_entry(entries, data.cFileName, (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
        }
    } while (ok && FindNextFileA(handle, &data));
    FindClose(handle);
    return ok;
}
#else
static int list_directory(const char *directory, entry_list *entries) {
    DIR *handle = opendir(directory);
    struct dirent *item;
    int ok = 1;
    if (handle == NULL) {
        return 0;
    }
    while (ok && (item = readdir(handle)) != NULL) {
        if (strcmp(item->d_name, ".") != 0 && strcmp(item->d_name, "..") != 0) {
            char *path = join(directory, item->d_name);
            struct stat status;
            ok = path != NULL && stat(path, &status) == 0 && add_entry(entries, item->d_name, S_ISDIR(status.st_mode));
            free(path);
        }
    }
    closedir(handle);
    return ok;
}
#endif

static int compare_entries(const void *left, const void *right) {
    return strcmp(((const entry *)left)->name, ((const entry *)right)->name);
}

static int has_json_extension(const char *name) {
    size_t length = strlen(name);
    return length > 5 && strcmp(name + length - 5, ".json") == 0;
}

static int walk(const char *directory, path_list *found, char *error, size_t error_size) {
    entry_list entries = {NULL, 0, 0};
    size_t at;
    int ok = 1;
    if (!list_directory(directory, &entries)) {
        entry_list_free(&entries);
        fail(error, error_size, "cannot read the directory", directory);
        return 0;
    }
    qsort(entries.items, entries.count, sizeof(entry), compare_entries);
    for (at = 0; ok && at < entries.count; at++) {
        char *path = join(directory, entries.items[at].name);
        if (path == NULL) {
            ok = 0;
        } else if (entries.items[at].is_directory) {
            ok = walk(path, found, error, error_size);
            free(path);
        } else if (has_json_extension(entries.items[at].name)) {
            ok = add_path(found, path);
        } else {
            free(path);
        }
    }
    entry_list_free(&entries);
    if (!ok && error[0] == '\0') {
        snprintf(error, error_size, "out of memory reading %s", directory);
    }
    return ok;
}

size_t walk_root_length(const char *directory) {
    size_t length = strlen(directory);
    while (length > 1 && (directory[length - 1] == '/' || directory[length - 1] == '\\')) {
        length--;
    }
    return length;
}

int walk_json_files(const char *directory, path_list *found, char *error, size_t error_size) {
    size_t length = walk_root_length(directory);
    char *trimmed = malloc(length + 1);
    int ok;
    found->paths = NULL;
    found->count = 0;
    error[0] = '\0';
    if (trimmed == NULL) {
        snprintf(error, error_size, "out of memory");
        return 0;
    }
    memcpy(trimmed, directory, length);
    trimmed[length] = '\0';
    ok = walk(trimmed, found, error, error_size);
    free(trimmed);
    if (!ok) {
        path_list_free(found);
    }
    return ok;
}

void path_list_free(path_list *list) {
    size_t at;
    for (at = 0; at < list->count; at++) {
        free(list->paths[at]);
    }
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
}
