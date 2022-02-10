#ifndef AST_H
#define AST_H

#include "lex.h"

typedef struct Unit Unit;

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
	
	VAR,
	DEREF,
	CAST,
	SUBSCRIPT,
	BINOP,
	CALL,
	MEMBER,
	
	PRINT,
	IF,
	WHILE,
	ASSIGN,
	RETURN,
	IMPORT,
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

typedef struct Expr {
	Kind kind;
	Token *start;
	int isconst;
	int islvalue;
	Type *dtype; // (cast)
	struct Expr *next; // next in a list
	
	union {
		int64_t ival; // int, bool
		char *string; // string
		Token *id; // var
		struct Expr *subexpr; // ptr, deref, cast, subscript, member
		struct Expr *left; // binop
		struct Expr *exprs; // array
		struct Expr *callee; // call
	};
	union {
		struct Expr *right; // binop
		struct Expr *index; // subscript
		int64_t length; // string, array
		Token *member_id; // member
	};
	Token *operator; // binop
} Expr;

typedef struct Stmt {
	Kind kind;
	Token *start;
	struct Stmt *next; // next in a list
	struct Scope *scope;
	int exported; // var, func, struct
	
	union {
		Expr *expr; // print, if, while, assign, return
		Expr *call; // call
		struct Stmt *func_body; // func
		struct Stmt *struct_body; // struct
		Token *filename; // import
	};
	union {
		Token *id; // var, func, struct
		struct Stmt *if_body; // if
		struct Stmt *while_body; // while
		Expr *target; // assign
		Unit *unit; // import
	};
	union {
		Type *dtype; // var, func
		struct Stmt *else_body; // if
		Token *imported_idents; // import
	};
	union {
		struct Stmt *next_decl; // next decl in scope (var, func, struct)
		struct Stmt *next_import; // next import in scope (import)
	};
	union {
		int64_t imported_ident_count; // import
	};
} Stmt;

typedef struct Scope {
	Stmt *first_decl;
	Stmt *last_decl;
	struct Scope *parent;
	Stmt *func;
	Stmt *struc;
	Stmt *first_import;
	Stmt *last_import;
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

Expr *new_expr(Kind kind, Token *start);
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

Stmt *clone_stmt(Stmt *stmt);
Stmt *new_stmt(Kind kind, Token *start, Scope *scope);
Stmt *new_vardecl(
	Token *id, Type *dtype, Expr *init, Token *start, Scope *scope
);
Stmt *new_assign(Expr *target, Expr *expr, Scope *scope);
Stmt *new_import(Token *filename, Unit *unit, Token *start, Scope *scope);

Stmt *lookup_flat_in(Token *id, Scope *scope);
Stmt *lookup_in(Token *id, Scope *scope);

#endif
