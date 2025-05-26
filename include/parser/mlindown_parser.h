#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H

#include "parser/mlindown_ast.h"
#include "parser/mlindown_token.h"


Node *parse_tokens(const TokenList *tokens);

#endif /* PARSER_PARSER_H */