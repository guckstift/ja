#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef enum {
	TY_INT64,
} Type;

typedef struct {
	Type type;
} TypeDesc;

typedef enum {
	EX_INT,
	EX_VAR,
} ExprType;

typedef struct Expr {
	ExprType type;
	Token *start;
	
	union {
		int64_t ival;
		Token *id;
	};
} Expr;

typedef enum {
	ST_PRINT, // expr
	ST_VARDECL, // id, dtype
} StmtType;

typedef struct Stmt {
	StmtType type;
	Token *start;
	struct Stmt *next; // next in a list
	struct Stmt *next_decl; // next declaration in scope
	
	Expr *expr;
	Token *id;
	TypeDesc *dtype;
} Stmt;

typedef struct {
	Stmt *stmts;
} Unit;

Unit *parse(Tokens *tokens);

#endif
