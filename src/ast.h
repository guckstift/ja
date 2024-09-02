#ifndef AST_H
#define AST_H

#include <stdint.h>
#include <stdbool.h>

#define KEYWORDS \
	_(bool) \
	_(else) \
	_(false) \
	_(function) \
	_(if) \
	_(int) \
	_(int8) \
	_(int16) \
	_(int32) \
	_(int64) \
	_(null) \
	_(print) \
	_(ptr) \
	_(string) \
	_(struct) \
	_(true) \
	_(uint) \
	_(uint8) \
	_(uint16) \
	_(uint32) \
	_(uint64) \
	_(var) \
	_(while) \

#define PUNCTS \
	_("=", ASSIGN) \
	_(":", COLON) \
	_(";", SEMICOLON) \
	_("{", LCURLY) \
	_("}", RCURLY) \
	_("[", LBRACK) \
	_("]", RBRACK) \
	_("(", LPAREN) \
	_(")", RPAREN) \
	_("*", STAR) \
	_("&", AMPERSAND) \

typedef struct {
	int64_t index;
	char *start;
	char *end;
	struct Token *first;
} Line;

typedef enum {
	TK_EOF,
	TK_IDENT,
	TK_INT,

	TK_KEYWORDS_START,
	#define _(x) KW_ ## x,
	KEYWORDS
	#undef _

	TK_PUNCTS_START,
	#define _(x, y) PT_ ## y,
	PUNCTS
	#undef _

	TY_INT8,
	TY_INT16,
	TY_INT32,
	TY_INT64,
	TY_INT = TY_INT64,
	TY_UINT8,
	TY_UINT16,
	TY_UINT32,
	TY_UINT64,
	TY_UINT = TY_UINT64,
	TY_STRING,
	TY_BOOL,
	TY_PTR,
	TY_ARRAY,
	TY_SLICE,

	EX_INT,
	EX_BOOL,
	EX_NULL,
	EX_VAR,
	EX_PTR,
	EX_DEREF,
	EX_CAST,
	EX_SUBSCRIPT,

	ST_VARDECL,
	ST_ASSIGN,
	ST_PRINT,
	ST_IF,
	ST_WHILE,
	ST_FUNCDECL,

	K_TYPE,
	K_EXPR,
	K_STMT,
} Kind;

typedef struct Token {
	Kind kind;
	char *start;
	int64_t length;
	int64_t index;
	Line *line;

	union {
		int64_t ival;
		struct Token *id;
	};
} Token;

// types

typedef struct Type {
	Kind kind;
	struct Type *subtype; // ptr, array
	int64_t length; // array
} Type;

// expressions

typedef struct Expr {
	Kind kind;
	Token *start;
	Type *type;
	int isconst : 1;
	int islvalue : 1;

	union {
		int64_t ival; // int
		bool bval; // bool
		Token *id; // var
		struct Expr *subexpr; // ptr, deref, cast, subscript
	};

	union {
		struct Stmt *decl; // var
		struct Expr *index; // subscript
	};
} Expr;

// statements

typedef struct Stmt {
	Kind kind;
	Token *start;
	Token *end;

	union {
		Token *id; // vardecl, funcdecl
		Expr *target; // assign
		Expr *cond; // if, while
	};

	union {
		Type *type; // vardecl
		struct Block *body; // if, while, funcdecl
	};

	union {
		Expr *init; // vardecl
		Expr *value; // assign, print
		struct Block *else_body; // if
	};
} Stmt;

// general

typedef struct Scope {
	struct Scope *parent;
	struct Stmt **decls;
} Scope;

typedef struct Block {
	struct Stmt **stmts;
	Scope *scope;
} Block;

typedef struct {
	char *srcfile;
	char *uid;
	char *hfile;
	char *cfile;
	char *ofile;
	char *src;
	Line *lines;
	Token *tokens;
	Block *root;
} Module;

typedef struct {
	Module *main;
	char *progfile;
} Project;

// functions

Token *new_token(Token value);
Type *new_type(Type value);
Expr *new_expr(Expr value);
Expr *expr_from_token(Token *token);
Expr *mock_int_var_expr();
Stmt *new_stmt(Stmt value);

#define create_token(k, ...) new_token((Token){.kind = (k), __VA_ARGS__})

#define create_type(k, ...)     new_type((Type){.kind = (k), __VA_ARGS__})
#define create_ptr_type(s)      create_type(TY_PTR, .subtype = (s))
#define create_array_type(s, l) create_type(TY_ARRAY, .subtype = (s), .length = (l))

#define create_expr(k, ...)          new_expr((Expr){.kind = (k), __VA_ARGS__})
#define create_int_expr(v, ...)      create_expr(EX_INT, .type = create_type(TY_INT), .isconst = 1, .ival = (v), __VA_ARGS__)
#define create_bool_expr(v, ...)     create_expr(EX_BOOL, .type = create_type(TY_BOOL), .isconst = 1, .bval = (v), __VA_ARGS__)
#define create_null_expr(...)        create_expr(EX_NULL, .type = create_type(TY_PTR), .isconst = 1, __VA_ARGS__)
#define create_var_expr(i, ...)      create_expr(EX_VAR, .islvalue = 1, .id = (i), __VA_ARGS__)
#define create_ptr_expr(se, ...)     create_expr(EX_PTR, .type = create_ptr_type(0), .subexpr = (se), __VA_ARGS__)
#define create_deref_expr(se, ...)   create_expr(EX_DEREF, .islvalue = 1, .subexpr = (se), __VA_ARGS__)
#define create_cast_expr(se, t, ...) create_expr(EX_CAST, .isconst = (se)->isconst, .subexpr = (se), .type = (t), __VA_ARGS__)

#define create_subscript_expr(a, i, ...) create_expr(EX_SUBSCRIPT, .islvalue = 1, .subexpr = (a), .index = (i), __VA_ARGS__)

#define create_stmt(k, ...)            new_stmt((Stmt){.kind = (k), __VA_ARGS__})
#define create_vardecl(i, ty, in, ...) create_stmt(ST_VARDECL, .id = (i), .type = (ty), .init = (in), __VA_ARGS__)
#define create_assign(ta, v, ...)      create_stmt(ST_ASSIGN, .target = (ta), .value = (v), __VA_ARGS__)
#define create_print(v, ...)           create_stmt(ST_PRINT, .value = (v), __VA_ARGS__)
#define create_if(c, b, e, ...)        create_stmt(ST_IF, .cond = (c), .body = (b), .else_body = (e), __VA_ARGS__)
#define create_while(c, b, ...)        create_stmt(ST_WHILE, .cond = (c), .body = (b), __VA_ARGS__)
#define create_funcdecl(i, b, ...)     create_stmt(ST_FUNCDECL, .id = (i), .body = (b), __VA_ARGS__)

Stmt *lookup_flat_in(Token *id, Scope *scope);
Stmt *lookup_in(Token *id, Scope *scope);

// variables

extern char *token_names[];

#endif