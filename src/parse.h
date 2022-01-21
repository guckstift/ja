#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include "lex.h"

typedef enum {
	TY_INT64,
	TY_UINT64,
	TY_BOOL,
	TY_PTR,
	TY_ARRAY,
} Type;

typedef struct TypeDesc {
	Type type;
	struct TypeDesc *subtype;
	uint64_t length;
} TypeDesc;

typedef enum {
	EX_INT, // ival
	EX_BOOL, // bval
	EX_VAR, // id
	EX_PTR, // subexpr
	EX_DEREF, // subexpr
	EX_CAST, // subexpr, (dtype)
	EX_SUBSCRIPT, // subexpr, index
	EX_BINOP, // left, right, operator
	EX_ARRAY, // exprs
} ExprType;

typedef struct Expr {
	ExprType type;
	Token *start;
	int isconst;
	int islvalue;
	TypeDesc *dtype;
	struct Expr *next; // next in a list
	
	union {
		int64_t ival;
		bool bval;
		Token *id;
		struct Expr *subexpr;
		struct Expr *left;
		struct Expr *exprs;
	};
	union {
		struct Expr *right;
		struct Expr *index;
	};
	Token *operator;
} Expr;

typedef enum {
	ST_PRINT, // expr
	ST_VARDECL, // id, dtype, expr, next_decl
	ST_IFSTMT, // expr, body, else_body
	ST_WHILESTMT, // expr, body
	ST_ASSIGN, // target, expr
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
		Expr *target;
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
