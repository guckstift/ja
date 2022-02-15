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
		struct Decl *typedecl; // struct
	};
} Type;

typedef enum {
	OL_CMP,
	OL_ADD,
	OL_MUL,
	OPLEVEL_COUNT,
} OpLevel;

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

#define Stmt_head \
	Kind kind; \
	Token *start; \
	struct Stmt *next; /* next stmt in a list */ \
	struct Scope *scope; \

typedef struct Import {
	Stmt_head
	
	Token *filename; // import
	Unit *unit; // import
	Token *imported_idents; // import
	struct Import *next_import; // next import in scope (import)
	int64_t imported_ident_count; // import
} Import;

typedef struct Decl {
	Stmt_head
	
	union {
		Expr *init; // var
		struct Stmt *body; // func, struct
	};
	
	int isproto; // func
	Token *id; // identifier of decl
	Type *dtype; // var, func
	struct Decl *next_decl; // next decl in scope
	int imported; // this decl is declared as a clone via import
	char *public_id; // for exported decls
	int exported; // this decl is exported
} Decl;

typedef struct If {
	Stmt_head
	
	Expr *expr;
	struct Stmt *if_body;
	struct Stmt *else_body;
} If;

typedef struct While {
	Stmt_head
	
	Expr *expr;
	struct Stmt *while_body;
} While;

typedef struct Assign {
	Stmt_head
	
	Expr *expr;
	Expr *target;
} Assign;

typedef struct Call {
	Stmt_head
	
	Expr *call;
} Call;

typedef struct Print {
	Stmt_head
	
	Expr *expr;
} Print;

typedef struct Return {
	Stmt_head
	
	Expr *expr;
} Return;

typedef struct Stmt {
	union {
		Import as_import;
		Decl as_decl;
		If as_if;
		While as_while;
		Assign as_assign;
		Call as_call;
		Print as_print;
		Return as_return;
		struct { Stmt_head };
	};
} Stmt;

typedef struct Scope {
	Decl *first_decl;
	Decl *last_decl;
	struct Scope *parent;
	Decl *func;
	Decl *struc;
	Import *first_import;
	Import *last_import;
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
Decl *new_vardecl(
	Token *id, Type *dtype, Expr *init, Token *start, Scope *scope
);
Assign *new_assign(Expr *target, Expr *expr, Scope *scope);
Import *new_import(Token *filename, Unit *unit, Token *start, Scope *scope);

Decl *lookup_flat_in(Token *id, Scope *scope);
Decl *lookup_in(Token *id, Scope *scope);

#endif
