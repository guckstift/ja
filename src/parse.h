#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef enum {
	EX_INT,
	EX_VAR,
} ExprType;

typedef struct Expr {
	ExprType type;
	Token *start;
	struct Expr *next; // next in a list
	
	union {
		int64_t ival;
		Token *id;
	};
} Expr;

typedef enum {
	ST_PRINT, // exprs
	ST_DECL, // id, expr
} StmtType;

typedef struct Stmt {
	StmtType type;
	Token *start;
	struct Stmt *next; // next in a list
	
	union {
		Expr *expr;
		Expr *exprs;
	};
	Token *id;
} Stmt;

typedef struct {
	Stmt *stmts;
} Unit;

Unit *parse(Token *tokens);

#endif
