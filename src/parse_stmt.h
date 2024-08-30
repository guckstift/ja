#ifndef PARSE_STMT_H
#define PARSE_STMT_H

#include "parse_type.h"
#include "parse_expr.h"
#include "ast.h"

Stmt *parse_stmt();
Stmt **parse_stmts();
Block *parse_block(Scope *scope);

#endif