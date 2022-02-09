#ifndef AST_H
#define AST_H

#include "lex.h"

typedef enum {
	NONE,
	INT8,
	INT16,
	INT32,
	INT64,
	INT = INT64,
	UINT8,
	UINT16,
	UINT32,
	UINT64,
	UINT = UINT64,
	BOOL,
	STRING,
	
	_PRIMKIND_COUNT,
	
	PTR,
	ARRAY,
	FUNC,
	STRUCT,
	
	_KIND_COUNT,
} Kind;

typedef struct Type {
	Kind kind;
	union {
		struct Type *subtype; // ptr
		struct Type *itemtype; // array
		struct Type *returntype; // func
		Token *id; // struct
	};
	union {
		int64_t length; // array (-1 = incomplete)
		struct Stmt *typedecl; // struct
	};
} Type;

typedef enum {
	EX_INT, // ival
	EX_BOOL, // ival
	EX_STRING, // string, length
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
	Type *dtype;
	struct Expr *next; // next in a list
	
	union {
		int64_t ival;
		char *string;
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
		Type *dtype;
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

Type *new_type(Kind kind);
Type *new_ptr_type(Type *subtype);
Type *new_array_type(int64_t length, Type *subtype);
Type *new_func_type(Type *returntype);

int type_equ(Type *dtype1, Type *dtype2);
int is_integer_type(Type *dtype);
int is_integral_type(Type *dtype);
int is_complete_type(Type *dtype);
int is_dynarray_ptr_type(Type *dtype);

Expr *new_expr(ExprType type, Token *start);
Expr *new_int_expr(int64_t val, Token *start);
Expr *new_string_expr(char *string, int64_t length, Token *start);
Expr *new_bool_expr(int64_t val, Token *start);
Expr *new_var_expr(Token *id, Type *dtype, Token *start);
Expr *new_array_expr(
	Expr *exprs, int64_t length, int isconst, Type *subtype, Token *start
);
Expr *new_subscript(Expr *subexpr, Expr *index);
Expr *new_cast_expr(Expr *subexpr, Type *dtype);
Expr *new_member_expr(Expr *subexpr, Token *member_id, Type *dtype);
Expr *new_deref_expr(Expr *subexpr);
Expr *new_ptr_expr(Expr *subexpr);

Stmt *new_stmt(StmtType type, Token *start, Scope *scope);
Stmt *new_assign(Expr *target, Expr *expr, Scope *scope);

Stmt *lookup_flat_in(Token *id, Scope *scope);
Stmt *lookup_in(Token *id, Scope *scope);

#endif
