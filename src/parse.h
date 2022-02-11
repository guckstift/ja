#ifndef PARSE_H
#define PARSE_H

#include "ast.h"

Node *parse_tree(Tokens *tokens);
Stmt *parse(Tokens *tokens, char *unit_id);

#endif
