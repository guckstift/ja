#ifndef AST_H
#define AST_H

#include "lex.h"

typedef enum {
	TY_INT64,
	TY_UINT8,
	TY_UINT64,
	TY_BOOL,
	TY_PTR,
	TY_ARRAY,
} Type;

typedef struct TypeDesc {
	Type type;
	struct TypeDesc *subtype;
	int64_t length;
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
	EX_ARRAY, // exprs, length
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
		Token *id;
		struct Expr *subexpr;
		struct Expr *left;
		struct Expr *exprs;
	};
	union {
		struct Expr *right;
		struct Expr *index;
		int64_t length;
	};
	Token *operator;
} Expr;

typedef enum {
	ST_PRINT, // expr
	ST_VARDECL, // id, dtype, expr, next_decl
	ST_FUNCDECL, // id, func_body
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
		struct Stmt *func_body;
	};
	struct Stmt *next_decl; // next declaration in scope
} Stmt;

typedef struct Scope {
	Stmt *first_decl;
	Stmt *last_decl;
	struct Scope *parent;
} Scope;

TypeDesc *new_type(Type type);
TypeDesc *new_ptr_type(TypeDesc *subtype);
TypeDesc *new_array_type(int64_t length, TypeDesc *subtype);
int type_equ(TypeDesc *dtype1, TypeDesc *dtype2);
int is_integer_type(TypeDesc *dtype);
int is_integral_type(TypeDesc *dtype);
Expr *new_expr(ExprType type, Token *start);
Expr *new_var_expr(Token *id, TypeDesc *dtype, Token *start);
Expr *new_int_expr(int64_t val, Token *start);
Expr *new_subscript(Expr *subexpr, Expr *index);
Stmt *new_stmt(StmtType type, Token *start, Scope *scope);
Stmt *new_assign(Expr *target, Expr *expr, Scope *scope);

#endif
