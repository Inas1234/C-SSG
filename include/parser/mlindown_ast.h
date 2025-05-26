#ifndef PARSER_AST_H
#define PARSER_AST_H

#include <stddef.h>

typedef enum {
    MLINDOWN_NODE_DOCUMENT,
    MLINDOWN_NODE_HEADING,
    MLINDOWN_NODE_PARAGRAPH,
    MLINDOWN_NODE_LIST,
    MLINDOWN_NODE_LIST_ITEM
} NodeType;

typedef struct Node {
    NodeType      type;         /* which kind of node */
    int           level;        /* for headings: 1–6; unused otherwise */
    char         *text;         /* leaf text (heading text, paragraph text, list‐item text) */
    struct Node **children;     /* child nodes (e.g. items in a list) */
    size_t        child_count;  /* number of children */
} Node;

Node *node_new(NodeType type, int level, const char *text);

void node_add_child(Node *parent, Node *child);

void node_free(Node *root);

#endif 