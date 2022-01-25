#ifndef PARSE_H
#define PARSE_H

#include "ast.h"

int is_integer_type(TypeDesc *dtype);
int is_integral_type(TypeDesc *dtype);
Stmt *parse(Tokens *tokens);

#endif
