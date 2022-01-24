#ifndef PARSE_H
#define PARSE_H

#include "ast.h"

typedef struct {
	Stmt *stmts;
} Unit;

int is_integer_type(TypeDesc *dtype);
int is_integral_type(TypeDesc *dtype);
Unit *parse(Tokens *tokens);

#endif
