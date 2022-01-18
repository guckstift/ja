#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include "lex.h"

typedef enum {
	TY_INT64,
	TY_BOOL,
	TY_PTR,
} Type;

typedef struct TypeDesc {
	Type type;
	struct TypeDesc *subtype;
} TypeDesc;

typedef enum {
	EX_INT,
	EX_BOOL,
	EX_VAR,
	EX_PTR,
} ExprType;

typedef struct Expr {
	ExprType type;
	Token *start;
	int isconst;
	TypeDesc *dtype;
	
	union {
		int64_t ival;
		bool bval;
		Token *id;
		struct Expr *expr;
	};
} Expr;

typedef enum {
	ST_PRINT, // expr
	ST_VARDECL, // id, dtype, expr, next_decl
	ST_IFSTMT, // expr, body, else_body
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
	union {
		TypeDesc *dtype;
		struct Stmt *else_body;
	};
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
