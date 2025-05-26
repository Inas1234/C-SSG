#ifndef PARSER_TOKEN_H
#define PARSER_TOKEN_H

#include <stddef.h>

typedef enum {
    TOKEN_HEADING,    /* an ATX heading line (e.g. "# Heading") */
    TOKEN_LIST_ITEM,  /* an unordered-list item line (e.g. "- Item") */
    TOKEN_BLANK,      /* an empty or all-whitespace line */
    TOKEN_TEXT        /* any other text line */
} TokenType;

/* A single line token */
typedef struct {
    TokenType type;
    int       level; /* for headings: number of leading '#'; else unused */
    char     *text;  /* the rest of the line, trimmed of marker & leading spaces */
} Token;

typedef struct {
    Token  *data;
    size_t  count;
} TokenList;

TokenList tokenize(const char *input);

void free_tokens(TokenList *list);

#endif 