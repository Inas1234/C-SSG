#include <stdlib.h>
#include <string.h>
#include "parser/mlindown_parser.h"

Node *parse_tokens(const TokenList *tokens) {
    Node *doc = node_new(MLINDOWN_NODE_DOCUMENT, 0, NULL);
    if (!doc) return NULL;

    size_t i = 0;
    while (i < tokens->count) {
        Token t = tokens->data[i];

        switch (t.type) {

        case TOKEN_HEADING: {
            Node *h = node_new(MLINDOWN_NODE_HEADING, t.level, t.text);
            node_add_child(doc, h);
            i++;
            break;
        }

        case TOKEN_BLANK:
            i++;
            break;

        case TOKEN_LIST_ITEM: {
            Node *ul = node_new(MLINDOWN_NODE_LIST, 0, NULL);
            while (i < tokens->count && tokens->data[i].type == TOKEN_LIST_ITEM) {
                Token li_t = tokens->data[i];
                Node *li = node_new(MLINDOWN_NODE_LIST_ITEM, 0, li_t.text);
                node_add_child(ul, li);
                i++;
            }
            node_add_child(doc, ul);
            break;
        }

        case TOKEN_TEXT: {
            size_t j = i, total = 0;
            while (j < tokens->count && tokens->data[j].type == TOKEN_TEXT) {
                total += strlen(tokens->data[j].text) + 1; // for newline or space
                j++;
            }
            char *buf = malloc(total + 1);
            if (!buf) return doc;  
            buf[0] = '\0';

            for (size_t k = i; k < j; k++) {
                strcat(buf, tokens->data[k].text);
                if (k + 1 < j) strcat(buf, "\n");
            }

            Node *p = node_new(MLINDOWN_NODE_PARAGRAPH, 0, buf);
            free(buf);
            node_add_child(doc, p);
            i = j;
            break;
        }

        default:
            i++;
            break;
        }
    }

    return doc;
}