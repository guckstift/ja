#ifndef AST_H
#define AST_H

#include "lex.h"

typedef enum {
	TY_NONE,
	TY_INT64,
	TY_UINT8,
	TY_UINT64,
	TY_BOOL,
	TY_PTR, // subtype
	TY_ARRAY, // subtype, length
	TY_FUNC, // returntype
	TY_STRUCT, // id, typedecl
} Type;

typedef struct TypeDesc {
	Type type;
	union {
		struct TypeDesc *subtype;
		struct TypeDesc *returntype;
		Token *id;
	};
	union {
		int64_t length; // -1 = incomplete
		struct Stmt *typedecl;
	};
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
	EX_CALL, // callee
	EX_MEMBER, // subexpr, member_id
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
		struct Expr *callee;
	};
	union {
		struct Expr *right;
		struct Expr *index;
		int64_t length;
		Token *member_id;
	};
	Token *operator;
} Expr;

typedef enum {
	ST_PRINT, // expr
	ST_VARDECL, // id, dtype, expr, next_decl
	ST_FUNCDECL, // id, dtype, func_body
	ST_STRUCTDECL, // id, struct_body
	ST_IFSTMT, // expr, body, else_body
	ST_WHILESTMT, // expr, body
	ST_ASSIGN, // target, expr
	ST_CALL, // call
	ST_RETURN, // expr
} StmtType;

typedef struct Stmt {
	StmtType type;
	Token *start;
	struct Stmt *next; // next in a list
	struct Scope *scope;
	
	union {
		Expr *expr;
		Expr *call;
		struct Stmt *func_body;
		struct Stmt *struct_body;
	};
	union {
		Token *id;
		struct Stmt *if_body;
		struct Stmt *while_body;
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
	Stmt *func;
	Stmt *struc;
} Scope;

TypeDesc *new_type(Type type);
TypeDesc *new_ptr_type(TypeDesc *subtype);
TypeDesc *new_array_type(int64_t length, TypeDesc *subtype);
TypeDesc *new_func_type(TypeDesc *returntype);

int type_equ(TypeDesc *dtype1, TypeDesc *dtype2);
int is_integer_type(TypeDesc *dtype);
int is_integral_type(TypeDesc *dtype);
int is_complete_type(TypeDesc *dtype);

Expr *new_expr(ExprType type, Token *start);
Expr *new_int_expr(int64_t val, Token *start);
Expr *new_bool_expr(int64_t val, Token *start);
Expr *new_var_expr(Token *id, TypeDesc *dtype, Token *start);
Expr *new_subscript(Expr *subexpr, Expr *index);
Expr *new_cast_expr(Expr *subexpr, TypeDesc *dtype);

Stmt *new_stmt(StmtType type, Token *start, Scope *scope);
Stmt *new_assign(Expr *target, Expr *expr, Scope *scope);

Stmt *lookup_flat_in(Token *id, Scope *scope);
Stmt *lookup_in(Token *id, Scope *scope);

#endif
