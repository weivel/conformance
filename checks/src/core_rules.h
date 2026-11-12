#ifndef CHECKS_CORE_RULES_H
#define CHECKS_CORE_RULES_H

#include <stddef.h>

#define MAXIMUM_TOKENS 8

typedef enum {
    RULE_ALLOW_EVERYWHERE,
    RULE_ALLOW_IN_MUTEX,
    RULE_ALLOW_FROM_COMPILER,
    RULE_ALLOW_FROM_STARTUP,
    RULE_STARTUP_EXPORT,
    RULE_FORBID,
    RULE_FORBID_NAME,
    RULE_FORBID_SOURCE,
    RULE_SOURCE_IN_MUTEX_ONLY
} rule_kind;

/**
 * How a source rule finds its name: `word` stands alone, `call` is followed by an opening parenthesis, `prefix` only
 * starts an identifier, `text` appears anywhere in any case. Symbol rules carry `FORM_SYMBOL`.
 */
typedef enum { FORM_SYMBOL, FORM_WORD, FORM_CALL, FORM_PREFIX, FORM_TEXT } rule_form;

typedef struct {
    rule_kind kind;
    rule_form form;
    char *reason;
    char *pattern;
} rule;

typedef struct {
    rule *items;
    size_t count;
    char *mutex_marker;
    char *mutex_source;
} rule_set;

/** Reads `<kind> <reason> <symbol>` and `<kind> <reason> <form> <name>` lines. Returns 1, or 0 with the cause printed. */
int rules_load(const char *path, rule_set *rules);
void rules_free(rule_set *rules);
size_t rules_count(const rule_set *rules, rule_kind kind);

/** `*text*` matches a substring in any case, `text*` a prefix, anything else the whole symbol. */
const rule *rules_find(const rule_set *rules, rule_kind kind, const char *symbol);

/** Splits on spaces and tabs in place, storing at most MAXIMUM_TOKENS; returns how many tokens there were. */
size_t split_tokens(char *line, char **tokens);

char *duplicate_text(const char *text, size_t length);

#endif
