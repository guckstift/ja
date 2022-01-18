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
	int isconst;
	
	union {
		int64_t ival;
		Token *id;
	};
} Expr;

typedef enum {
	ST_PRINT, // expr
	ST_VARDECL, // id, dtype, expr, next_decl
	ST_IFSTMT, // expr, body
} StmtType;

typedef struct Stmt {
	StmtType type;
	Token *start;
	struct Stmt *next; // next in a list
	struct Scope *scope;
	
	Expr *expr;
	union {
		Token *id;
		struct Stmt *body;
	};
	TypeDesc *dtype;
	struct Stmt *next_decl; // next declaration in scope
} Stmt;

typedef struct Scope {
	Stmt *first_decl;
	Stmt *last_decl;
	struct Scope *parent;
} Scope;

typedef struct {
	Stmt *stmts;
} Unit;

Unit *parse(Tokens *tokens);

#endif
