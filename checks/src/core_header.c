#include "core_header.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header_tokens.h"
#include "tree.h"

static const char *const OPAQUE_WORDS[] = {"__attribute__", "__attribute", "__declspec", "_Alignas", "alignas",
                                           "__pragma", "_Pragma", "__asm__", "__asm", "asm"};
static const char *const OPERATOR_WORDS[] = {"sizeof", "_Alignof", "alignof", "__alignof", "__alignof__",
                                             "__typeof__", "__typeof", "typeof", "_Generic", "_Static_assert",
                                             "static_assert", "offsetof"};
static const char *const QUALIFIERS[] = {"const", "volatile", "restrict", "__restrict", "__restrict__", "_Atomic",
                                         "__ptr32", "__ptr64", "__unaligned", "__sptr", "__uptr", "_Nonnull",
                                         "_Nullable", "_Null_unspecified"};
static const char *const CALLING_CONVENTIONS[] = {"__cdecl", "_cdecl", "__stdcall", "_stdcall", "__fastcall",
                                                  "_fastcall", "__vectorcall", "__thiscall", "__clrcall", "__pascal",
                                                  "_pascal", "__regcall", "__syscall", "__far", "__near"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

typedef struct {
    const header_token *items;
    size_t count;
} token_view;

static int is_text(const token_view *tokens, size_t at, const char *text) {
    return at < tokens->count && tokens->items[at].length == strlen(text) &&
           strncmp(tokens->items[at].text, text, tokens->items[at].length) == 0;
}

static int is_identifier(const token_view *tokens, size_t at) {
    return at < tokens->count && (isalpha((unsigned char)tokens->items[at].text[0]) || tokens->items[at].text[0] == '_');
}

static int is_one_of(const token_view *tokens, size_t at, const char *const *words, size_t count) {
    size_t index;
    for (index = 0; index < count; index++) {
        if (is_text(tokens, at, words[index])) {
            return 1;
        }
    }
    return 0;
}

static int is_operator_word(const token_view *tokens, size_t at) {
    return is_one_of(tokens, at, OPERATOR_WORDS, COUNT_OF(OPERATOR_WORDS)) ||
           (at < tokens->count && tokens->items[at].length > 10 &&
            strncmp(tokens->items[at].text, "__builtin_", 10) == 0);
}

/** The index of the token closing the group `(` or `[` at `open` opens, or the token count when it never closes. */
static size_t closing_of(const token_view *tokens, size_t open) {
    const char *opening = is_text(tokens, open, "(") ? "(" : "[";
    const char *closing = opening[0] == '(' ? ")" : "]";
    size_t depth = 0;
    size_t at;
    for (at = open; at < tokens->count; at++) {
        depth += is_text(tokens, at, opening);
        if (is_text(tokens, at, closing) && --depth == 0) {
            return at;
        }
    }
    return tokens->count;
}

/** Past an attribute, alignment or pragma group starting at `at`, or `at` itself when none starts there. */
static size_t past_opaque(const token_view *tokens, size_t at) {
    if (is_one_of(tokens, at, OPAQUE_WORDS, COUNT_OF(OPAQUE_WORDS)) && is_text(tokens, at + 1, "(")) {
        return closing_of(tokens, at + 1) + 1;
    }
    return at;
}

/**
 * Whether the parenthesized group from `open` to `close`, which a parameter list follows, is a pointer declarator:
 * words, then one or more stars with qualifiers or attributes among them, then a declarator or nothing. A group with
 * no declarator after its stars is a pointer only when the words before them are calling conventions, so the cast
 * `(uint32_t *)(x)` is not one and the abstract `(__cdecl *)(int)` is.
 */
static int is_pointer_declarator(const token_view *tokens, size_t open, size_t close) {
    size_t at = open + 1;
    int only_conventions = 1;
    while (at < close && !is_text(tokens, at, "*")) {
        size_t past = past_opaque(tokens, at);
        if (past != at) {
            at = past;
            continue;
        }
        if (!is_identifier(tokens, at)) {
            return 0;
        }
        only_conventions = only_conventions && (is_one_of(tokens, at, CALLING_CONVENTIONS, COUNT_OF(CALLING_CONVENTIONS)) ||
                                                is_one_of(tokens, at, QUALIFIERS, COUNT_OF(QUALIFIERS)));
        at++;
    }
    if (at >= close) {
        return 0;
    }
    while (at < close && (is_text(tokens, at, "*") || is_one_of(tokens, at, QUALIFIERS, COUNT_OF(QUALIFIERS)) ||
                          past_opaque(tokens, at) != at)) {
        at = past_opaque(tokens, at) != at ? past_opaque(tokens, at) : at + 1;
    }
    return at < close || only_conventions;
}

static int opens_a_function_pointer(const token_view *tokens, size_t at) {
    size_t close;
    if (!is_text(tokens, at, "(") || (at > 0 && is_one_of(tokens, at - 1, OPAQUE_WORDS, COUNT_OF(OPAQUE_WORDS)))) {
        return 0;
    }
    close = closing_of(tokens, at);
    return close < tokens->count && is_text(tokens, close + 1, "(") && is_pointer_declarator(tokens, at, close);
}

/** Whether the identifier at `at` names a function: a type stands before it and a parameter list follows it. */
static int names_a_function(const token_view *tokens, size_t at) {
    return at > 0 && is_identifier(tokens, at) && is_text(tokens, at + 1, "(") && !is_operator_word(tokens, at) &&
           !is_one_of(tokens, at, OPAQUE_WORDS, COUNT_OF(OPAQUE_WORDS)) &&
           (is_identifier(tokens, at - 1) || is_text(tokens, at - 1, "*"));
}

typedef struct {
    size_t *lines;
    size_t count;
} hit_lines;

static int add_hit(hit_lines *hits, size_t line) {
    size_t *grown;
    size_t at;
    for (at = 0; at < hits->count; at++) {
        if (hits->lines[at] == line) {
            return 1;
        }
    }
    grown = realloc(hits->lines, (hits->count + 1) * sizeof *grown);
    if (grown == NULL) {
        return 0;
    }
    hits->lines = grown;
    grown[hits->count++] = line;
    return 1;
}

/** Hits every pointer declarator, and every parameter of function type: a function named inside parentheses. */
static int find_declarator_hits(const token_view *tokens, hit_lines *hits) {
    size_t at = 0;
    int parentheses = 0;
    int brackets = 0;
    int ok = 1;
    while (ok && at < tokens->count) {
        size_t past = past_opaque(tokens, at);
        if (past != at) {
            at = past;
            continue;
        }
        if (opens_a_function_pointer(tokens, at) ||
            (parentheses > 0 && brackets == 0 && names_a_function(tokens, at))) {
            ok = add_hit(hits, tokens->items[at].line);
        }
        parentheses += is_text(tokens, at, "(") - is_text(tokens, at, ")");
        brackets += is_text(tokens, at, "[") - is_text(tokens, at, "]");
        at++;
    }
    return ok;
}

/** The end of the typedef at `start`, its semicolon, and `*tail` where its declarators begin, past any body. */
static size_t typedef_end(const token_view *tokens, size_t start, size_t *tail) {
    int depth = 0;
    size_t at;
    *tail = start + 1;
    for (at = start + 1; at < tokens->count; at++) {
        if (is_text(tokens, at, "(") || is_text(tokens, at, "[") || is_text(tokens, at, "{")) {
            depth++;
        } else if (is_text(tokens, at, ")") || is_text(tokens, at, "]") || is_text(tokens, at, "}")) {
            depth--;
            if (depth == 0 && is_text(tokens, at, "}")) {
                *tail = at + 1;
            }
        } else if (depth == 0 && is_text(tokens, at, ";")) {
            return at;
        }
    }
    return at;
}

static int holds_a_pointer_declarator(const token_view *tokens, size_t start, size_t end) {
    size_t at;
    for (at = start; at < end; at++) {
        if (opens_a_function_pointer(tokens, at)) {
            return 1;
        }
    }
    return 0;
}

/** Whether the declarators from `start` to `end` name a function outside every group, attributes skipped. */
static int declares_a_function_type(const token_view *tokens, size_t start, size_t end) {
    size_t at = start;
    while (at < end) {
        size_t past = past_opaque(tokens, at);
        if (past != at) {
            at = past;
        } else if (is_text(tokens, at, "(") || is_text(tokens, at, "[")) {
            at = closing_of(tokens, at) + 1;
        } else if (names_a_function(tokens, at)) {
            return 1;
        } else {
            at++;
        }
    }
    return 0;
}

/** Hits every typedef of a function type, at the line of its `typedef`. */
static int find_typedef_hits(const token_view *tokens, hit_lines *hits) {
    size_t at;
    for (at = 0; at < tokens->count; at++) {
        if (is_text(tokens, at, "typedef")) {
            size_t tail;
            size_t end = typedef_end(tokens, at, &tail);
            if (!holds_a_pointer_declarator(tokens, at, end) && declares_a_function_type(tokens, tail, end) &&
                !add_hit(hits, tokens->items[at].line)) {
                return 0;
            }
        }
    }
    return 1;
}

static int compare_lines(const void *left, const void *right) {
    size_t first = *(const size_t *)left;
    size_t second = *(const size_t *)right;
    return (first > second) - (first < second);
}

static int scan_header(const rule *header_rule, const char *root, size_t *offenders) {
    char *path = tree_join(root, header_rule->pattern);
    char error[2048];
    header_tokens read;
    token_view tokens;
    hit_lines hits = {NULL, 0};
    size_t at;
    int ok;
    if (path == NULL || !header_tokens_read(path, &read, error, sizeof error)) {
        if (path != NULL && strncmp(error, "cannot read", strlen("cannot read")) == 0) {
            fprintf(stderr, "no %s under %s, so its declarations were not checked\n", header_rule->pattern, root);
        } else {
            fprintf(stderr, "%s, so its declarations were not checked\n", path == NULL ? "out of memory" : error);
        }
        free(path);
        return 0;
    }
    free(path);
    tokens.items = read.items;
    tokens.count = read.count;
    ok = find_declarator_hits(&tokens, &hits) && find_typedef_hits(&tokens, &hits);
    if (hits.count > 0) {
        qsort(hits.lines, hits.count, sizeof *hits.lines, compare_lines);
    }
    for (at = 0; at < hits.count; at++) {
        fprintf(stderr, "%s:%zu: declares a function pointer type, and the core never calls out (%s)\n",
                header_rule->pattern, hits.lines[at], header_rule->reason);
    }
    *offenders += hits.count;
    free(hits.lines);
    header_tokens_free(&read);
    return ok;
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
