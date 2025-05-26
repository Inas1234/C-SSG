#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser/mlindown_token.h"

static char *ltrim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}

static void append_token(TokenList *tl, Token t) {
    Token *new_data = realloc(tl->data, (tl->count + 1) * sizeof(Token));
    if (!new_data) {
        /* OOM: drop the token */
        if (t.text) free(t.text);
        return;
    }
    tl->data = new_data;
    tl->data[tl->count++] = t;
}

TokenList tokenize(const char *input) {
    TokenList tl = { .data = NULL, .count = 0 };
    const char *line_start = input;
    while (*line_start) {
        const char *newline = strchr(line_start, '\n');
        size_t len = newline ? (size_t)(newline - line_start) : strlen(line_start);

        char *buf = malloc(len + 1);
        memcpy(buf, line_start, len);
        buf[len] = '\0';

        Token tok = { .type = TOKEN_TEXT, .level = 0, .text = NULL };
        char *trimmed = buf;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0') {
            tok.type = TOKEN_BLANK;
            tok.text = NULL;
            free(buf);
        }
        else if (trimmed[0] == '#') {
            int lvl = 0;
            while (trimmed[lvl] == '#' && lvl < 6) lvl++;
            char *rest = trimmed + lvl;
            if (*rest == ' ') rest++;
            tok.type  = TOKEN_HEADING;
            tok.level = lvl;
            tok.text  = strdup(rest);
            free(buf);
        }
        else if (trimmed[0] == '-' && isspace((unsigned char)trimmed[1])) {
            char *rest = trimmed + 2;  /* skip "- " */
            tok.type  = TOKEN_LIST_ITEM;
            tok.level = 0;
            tok.text  = strdup(ltrim(rest));
            free(buf);
        }
        else {
            tok.type  = TOKEN_TEXT;
            tok.level = 0;
            tok.text  = buf;  /* take ownership */
        }

        append_token(&tl, tok);

        if (!newline) break;
        line_start = newline + 1;
    }

    return tl;
}

void free_tokens(TokenList *tl) {
    if (!tl) return;
    for (size_t i = 0; i < tl->count; i++) {
        free(tl->data[i].text);
    }
    free(tl->data);
    tl->data  = NULL;
    tl->count = 0;
}