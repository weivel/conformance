#include "core_header.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

static int is_identifier_character(char character) {
    return isalnum((unsigned char)character) || character == '_';
}

static size_t skip_space(const char *text, size_t at) {
    while (text[at] != '\0' && isspace((unsigned char)text[at])) {
        at++;
    }
    return at;
}

static size_t skip_identifier(const char *text, size_t at) {
    if (isdigit((unsigned char)text[at])) {
        return at;
    }
    while (is_identifier_character(text[at])) {
        at++;
    }
    return at;
}

static int is_word_at(const char *text, size_t at, const char *word) {
    size_t length = strlen(word);
    return strncmp(text + at, word, length) == 0 && !is_identifier_character(text[at + length]) &&
           (at == 0 || !is_identifier_character(text[at - 1]));
}

static void blank(char *text, size_t at) {
    if (text[at] != '\n') {
        text[at] = ' ';
    }
}

/**
 * Blanks from `at`, which starts with `opening`, through the first `closing` after it, keeping newlines. A backslash
 * escapes the next character in a literal and not in a comment.
 */
static size_t blank_span(char *text, size_t at, const char *opening, const char *closing, int escapes) {
    size_t closing_length = strlen(closing);
    size_t end = at + strlen(opening);
    while (text[end] != '\0' && strncmp(text + end, closing, closing_length) != 0) {
        end += escapes && text[end] == '\\' && text[end + 1] != '\0' ? 2 : 1;
    }
    end = text[end] == '\0' ? end : end + closing_length;
    for (; at < end; at++) {
        blank(text, at);
    }
    return end;
}

/** Blanks every comment and every string and character literal in place, keeping each newline so lines still count. */
static void blank_comments_and_literals(char *text) {
    size_t at = 0;
    while (text[at] != '\0') {
        if (strncmp(text + at, "/*", 2) == 0) {
            at = blank_span(text, at, "/*", "*/", 0);
        } else if (strncmp(text + at, "//", 2) == 0) {
            at = blank_span(text, at, "//", "\n", 0);
        } else if (text[at] == '"') {
            at = blank_span(text, at, "\"", "\"", 1);
        } else if (text[at] == '\'') {
            at = blank_span(text, at, "'", "'", 1);
        } else {
            at++;
        }
    }
}

/**
 * Whether the parenthesis at `at` opens a pointer declarator that a parameter list follows: `(*name)(`, `(*)(`,
 * `(* const name)(`, or `(CALL *name)(` with one word before the star.
 *
 * @implNote The word form asks for a name after the star, because `(uint32_t *)(x)` is a cast and not a declarator.
 */
static int opens_a_function_pointer(const char *text, size_t at) {
    size_t cursor = skip_space(text, at + 1);
    size_t word_end = skip_identifier(text, cursor);
    int name_required = word_end > cursor;
    size_t name_end;
    if (name_required) {
        cursor = skip_space(text, word_end);
    }
    if (text[cursor] != '*') {
        return 0;
    }
    cursor = skip_space(text, cursor + 1);
    if (is_word_at(text, cursor, "const")) {
        cursor = skip_space(text, cursor + strlen("const"));
    }
    name_end = skip_identifier(text, cursor);
    if (name_required && name_end == cursor) {
        return 0;
    }
    cursor = skip_space(text, name_end);
    if (text[cursor] != ')') {
        return 0;
    }
    return text[skip_space(text, cursor + 1)] == '(';
}

static int holds_a_pointer_declarator(const char *text, size_t start, size_t end) {
    size_t at;
    for (at = start; at < end; at++) {
        if (text[at] == '(' && opens_a_function_pointer(text, at)) {
            return 1;
        }
    }
    return 0;
}

/** The end of the typedef starting at `start`, its closing semicolon, and `*tail` where its declarators begin. */
static size_t typedef_end(const char *text, size_t start, size_t *tail) {
    size_t at = start + strlen("typedef");
    int braces = 0;
    int parentheses = 0;
    *tail = at;
    for (; text[at] != '\0'; at++) {
        char character = text[at];
        braces += (character == '{') - (character == '}');
        parentheses += (character == '(') - (character == ')');
        if (character == '}' && braces == 0) {
            *tail = at + 1;
        }
        if (character == ';' && braces == 0 && parentheses == 0) {
            return at;
        }
    }
    return at;
}

/** Whether a declarator from `start` to `end` names a function: a name that a parameter list follows, outside any. */
static int declares_a_function(const char *text, size_t start, size_t end) {
    int parentheses = 0;
    size_t at = start;
    while (at < end) {
        if (parentheses == 0 && is_identifier_character(text[at]) && (at == 0 || !is_identifier_character(text[at - 1]))) {
            size_t name_end = skip_identifier(text, at);
            if (name_end > at && text[skip_space(text, name_end)] == '(') {
                return 1;
            }
            at = name_end > at ? name_end : at + 1;
            continue;
        }
        parentheses += (text[at] == '(') - (text[at] == ')');
        at++;
    }
    return 0;
}

static size_t line_of(const char *text, size_t position) {
    size_t line = 1;
    size_t at;
    for (at = 0; at < position; at++) {
        line += text[at] == '\n';
    }
    return line;
}

static void report_function_pointer(const rule *header_rule, const char *text, size_t at) {
    fprintf(stderr, "%s:%zu: declares a function pointer type, and the core never calls out (%s)\n",
            header_rule->pattern, line_of(text, at), header_rule->reason);
}

static size_t count_function_pointers(const rule *header_rule, const char *text) {
    size_t offenders = 0;
    size_t at;
    for (at = 0; text[at] != '\0'; at++) {
        if (text[at] == '(' && opens_a_function_pointer(text, at)) {
            report_function_pointer(header_rule, text, at);
            offenders++;
        } else if (is_word_at(text, at, "typedef")) {
            size_t tail;
            size_t end = typedef_end(text, at, &tail);
            if (!holds_a_pointer_declarator(text, at, end) && declares_a_function(text, tail, end)) {
                report_function_pointer(header_rule, text, at);
                offenders++;
            }
        }
    }
    return offenders;
}

static int scan_header(const rule *header_rule, const char *root, size_t *offenders) {
    char *path = tree_join(root, header_rule->pattern);
    char *text = NULL;
    size_t size;
    int read = path != NULL && tree_read_file(path, &text, &size);
    if (!read) {
        fprintf(stderr, "no %s under %s, so its declarations were not checked\n", header_rule->pattern, root);
    } else {
        blank_comments_and_literals(text);
        *offenders += count_function_pointers(header_rule, text);
    }
    free(text);
    free(path);
    return read;
}

int core_header_scan(const rule_set *rules, const char *root, size_t *offenders) {
    size_t at;
    for (at = 0; at < rules->count; at++) {
        if (rules->items[at].kind == RULE_PUBLIC_HEADER && !scan_header(&rules->items[at], root, offenders)) {
            return 0;
        }
    }
    return 1;
}
