#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef enum {
	EX_INT,
	EX_VAR,
} ExprType;

typedef struct Expr {
	ExprType type;
	struct Expr *next; // next in a list
	union {
		int64_t ival;
	};
} Expr;

typedef enum {
	ST_PRINT, // exprs
} StmtType;

typedef struct {
	StmtType type;
	Expr *exprs;
} Stmt;

void parse(Token *tokens);

#endif
