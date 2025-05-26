#include <stdlib.h>
#include <string.h>
#include "parser/mlindown_ast.h"

Node *node_new(NodeType type, int level, const char *text) {
    Node *n = malloc(sizeof(Node));
    if (!n) return NULL;

    n->type        = type;
    n->level       = level;
    n->child_count = 0;
    n->children    = NULL;

    if (text) {
        n->text = strdup(text);
        if (!n->text) {
            free(n);
            return NULL;
        }
    } else {
        n->text = NULL;
    }

    return n;
}

void node_add_child(Node *parent, Node *child) {
    if (!parent || !child) return;

    size_t new_count = parent->child_count + 1;
    Node **new_children = realloc(parent->children, new_count * sizeof(Node *));
    if (!new_children) return;  /* OOM: drop the child */

    parent->children    = new_children;
    parent->children[parent->child_count] = child;
    parent->child_count = new_count;
}

void node_free(Node *root) {
    if (!root) return;

    for (size_t i = 0; i < root->child_count; i++) {
        node_free(root->children[i]);
    }
    free(root->children);

    if (root->text) free(root->text);
    free(root);
}