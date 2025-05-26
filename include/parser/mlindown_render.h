#ifndef PARSER_RENDER_H
#define PARSER_RENDER_H

#include <stdio.h>
#include "parser/mlindown_ast.h"

void render_html(const Node *node, FILE *out);

char *render_html_str(const Node *node);

#endif 