#ifndef ast_H
#define ast_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define ast_C
#endif

#include <stdint.h>
#include <stdbool.h>

#define KEYWORDS \
	_(bool) \
	_(else) \
	_(false) _(function) \
	_(if) _(import) _(int) _(int8) _(int16) _(int32) _(int64) \
	_(null) \
	_(print) _(ptr) \
	_(return) \
	_(string) _(struct) \
	_(true) \
	_(uint) _(uint8) _(uint16) _(uint32) _(uint64) \
	_(var) _(void) \
	_(while) \

#define PUNCTS \
	_("=", ASSIGN) \
	_(":", COLON) \
	_(";", SEMICOLON) \
	_(",", COMMA) \
	_("+", PLUS) \
	_("{", LCURLY) _("}", RCURLY) \
	_("[", LBRACK) _("]", RBRACK) \
	_("(", LPAREN) _(")", RPAREN) \
	_("*", STAR) \
	_("&", AMPERSAND) \

typedef enum {
	TK_EOF,
	TK_INT,
	TK_IDENT,
	TK_STRING,

	TK_KEYWORDS_START,
	#define _(x) KW_ ## x,
	KEYWORDS
	#undef _

	TK_PUNCTS_START,
	#define _(x, y) PT_ ## y,
	PUNCTS
	#undef _

	K_TYPE,

	TY_VOID,
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
	TY_BOOL,
	TY_STRING,
	TY_PTR,
	TY_ARRAY,
	TY_FUNC,
	TY_SLICE,

	K_EXPR,

	EX_INT,
	EX_BOOL,
	EX_NULL,
	EX_STRING,
	EX_VAR,
	EX_PTR,
	EX_DEREF,
	EX_CAST,
	EX_SUBSCRIPT,
	EX_CALL,

	K_STMT,

	ST_VARDECL,
	ST_ASSIGN,
	ST_PRINT,
	ST_IF,
	ST_WHILE,
	ST_FUNCDECL,
	ST_RETURN,
	ST_CALL,
	ST_IMPORT,
} Kind;

//
// tokens
//

typedef struct Token Token;
typedef struct Type Type;
typedef struct Expr Expr;
typedef struct Decl Decl;
typedef struct Block Block;
typedef struct Scope Scope;
typedef struct Module Module;

typedef struct {
	int64_t index;
	char *start;
	char *end;
	Token *first;
} Line;

typedef struct Token {
	Kind kind;
	char *start;
	int64_t length;
	Line *line;
	uint64_t hash;

	union {
		int64_t ival;
		Token *uid;
		uint64_t slen;
		char *sval;
	};
} Token;

typedef struct {
	Token *key;
	void *value;
	int probe;
} Slot;

typedef struct {
	Slot *slots;
	uint64_t usage;
} Table;

//
// types
//

typedef struct Type {
	Kind kind;
	void *next;
	int complete : 1; // func

	union {
		Type *subtype; // ptr, array
		Type *returntype; // func
	};

	union {
		int64_t length; // array
		Type **paramtypes; // func
	};
} Type;

//
// expressions
//

typedef enum {
	OL_OR,
	OL_AND,
	OL_CMP,
	OL_ADD,
	OL_MUL,

	OPLEVEL_COUNT,
} OpLevel;

typedef struct Expr {
	Kind kind;
	Token *start;
	Type *type; // set first in analyze phase
	void *next;
	int isconst : 1;
	int islvalue : 1;

	union {
		int64_t ival; // int
		bool bval; // bool
		char *sval; // string
		Token *uid; // var
		Expr *subexpr; // ptr, deref, cast, subscript
		Expr *callee; // call
	};

	union {
		Decl *decl; // var
		Expr *index; // subscript
		Expr **args; // call
	};
} Expr;

//
// statements
//

#define STMT_HEAD \
	Kind kind; \
	Token *start; \
	Token *end; \
	void *next; \

typedef struct Stmt {
	STMT_HEAD

	union {
		Expr *target; // assign
		Expr *cond; // if, while
		Expr *call; // call
		char *filename; // import
	};

	union {
		Expr *value; // assign, return
		Expr *arg; // print
		Block *body; // if, while
		Module *module; // import
	};

	union {
		Block *else_body; // if
	};
} Stmt;

//
// declarations
//

typedef struct Decl {
	struct { STMT_HEAD } stmt;
	Token *uid;
	Type *type; // vardecl's data type or TY_FUNC for funcdecl
	Scope *scope;
	void *next; // next in scope; at the beginning of block analyzation linked list is turned into array

	union {
		Expr *init; // vardecl
		Block *body; // funcdecl
		Scope *funcscope; // funcdecl just after p_funchead; migrates into body later
	};

	Decl **params; // funcdecl
	//Decl **deps; // funcdecl
	Table deps; // funcdecl
} Decl;

//
// general
//

typedef struct Scope {
	Scope *parent;
	Decl **decls;
	Decl *first;
	Decl *last;
	int64_t count;
	uint64_t mock_id_counter;
	Stmt **imports;
} Scope;

typedef struct Block {
	Stmt **stmts;
	Scope *scope;
} Block;

typedef struct Module {
	char *srcfile; // absolute path of source file
	char *srcdir;  // absolute path of dir cotaining source file
	char *uid;     // identifier made out of srcfile
	char *hfile;   // abs. path of C-Header file
	char *cfile;   // abs. path of C-Source file
	char *ofile;   // abs. path of object file
	char *maino;   // abs. path of main function object file
	char *src;     // the contents in srcfile
	Line *lines;
	Token *tokens;
	Table idset;
	Block *root;
} Module;

typedef struct {
	Module *main;
	char *progfile;
	Module **modules;
} Project;

//
// functions
//

Token *new_token(Token value);
Type *new_type(Type value);
Expr *new_expr(Expr value);
Decl *new_decl(Decl value);
Stmt *new_stmt(Stmt value);
int scope_contains_scope(Scope *upper, Scope *lower);
Block *create_block(Stmt **stmts, Scope *scope);

#define create_token(k, ...) new_token((Token){.kind = (k), __VA_ARGS__})

#define create_type(k, ...)     new_type((Type){.kind = (k), __VA_ARGS__})
#define create_ptr_type(s)      create_type(TY_PTR, .subtype = (s))
#define create_array_type(s, l) create_type(TY_ARRAY, .subtype = (s), .length = (l))
#define create_func_type(r, p)  create_type(TY_FUNC, .returntype = (r), .paramtypes = (p))

#define create_expr(k, a...)               new_expr((Expr){.kind = (k), a})
#define create_int_expr(i, a...)           create_expr(EX_INT, .type = create_type(TY_INT), .isconst = 1, .ival = (i), a)
#define create_bool_expr(b, a...)          create_expr(EX_BOOL, .type = create_type(TY_BOOL), .isconst = 1, .bval = (b), a)
#define create_null_expr(a...)             create_expr(EX_NULL, .type = create_ptr_type(create_type(TY_VOID)), .isconst = 1, a)
#define create_string_expr(s, a...)        create_expr(EX_STRING, .type = create_type(TY_STRING), .isconst = 1, .sval = (s), a)
#define create_var_expr(i, a...)           create_expr(EX_VAR, .islvalue = 1, .uid = (i), a)
#define create_ptr_expr(se, a...)          create_expr(EX_PTR, .type = create_type(TY_PTR), .subexpr = (se), a)
#define create_deref_expr(se, a...)        create_expr(EX_DEREF, .islvalue = 1, .subexpr = (se), a)
#define create_cast_expr(se, t, a...)      create_expr(EX_CAST, .type = (t), .isconst = (se)->isconst, .subexpr = (se), a)
#define create_call_expr(c, ar, a...)      create_expr(EX_CALL, .callee = (c), .args = (ar), a)
#define create_subscript_expr(se, i, a...) create_expr(EX_SUBSCRIPT, .islvalue = 1, .subexpr = (se), .index = (i), a)

#define create_stmt(k, a...)           new_stmt((Stmt){.kind = (k), a})
#define create_assign(ta, v, a...)     create_stmt(ST_ASSIGN, .target = (ta), .value = (v), a)
#define create_print(v, a...)          create_stmt(ST_PRINT, .value = (v), a)
#define create_if(c, b, e, a...)       create_stmt(ST_IF, .cond = (c), .body = (b), .else_body = (e), a)
#define create_while(c, b, a...)       create_stmt(ST_WHILE, .cond = (c), .body = (b), a)
#define create_return(v, a...)         create_stmt(ST_RETURN, .value = (v), a)
#define create_call(c, ar, a...)       create_stmt(ST_CALL, .call = (c), a)
#define create_import(f, a...)         create_stmt(ST_IMPORT, .filename = (f), a)

#define create_decl(k, i, t, a...)     new_decl((Decl){.stmt.kind = (k), .uid = (i), .type = (t), a})
#define create_vardecl(i, in, t, a...) create_decl(ST_VARDECL, i, t, .init = (in), a)

#define create_funcdecl(i, r, p, s, a...) \
	create_decl(ST_FUNCDECL, i, create_func_type(r, 0), .params = (p), .funcscope = (s), a)

Decl *lookup_flat_in(Token *uid, Scope *scope);
Decl *lookup_in(Token *uid, Scope *scope);

//
// variables
//

extern char *token_names[];

#endif
#ifdef ast_C
#undef ast_C

#include <stdio.h>
#include "array.c"
#include "arena.c"
#include "print.c"

char *token_names[] = {
	"end of file",
	"integer literal",
	"identifier",
	"string literal",

	"<TK_KEYWORDS_START>",
	#define _(x) "keyword " #x,
	KEYWORDS
	#undef _

	"<TK_PUNCTS_START>",
	#define _(x, y) "'" x "'",
	PUNCTS
	#undef _
};

static Type stock_types[] = {
	[TY_VOID - K_TYPE] = {.kind = TY_VOID},
	[TY_INT8 - K_TYPE] = {.kind = TY_INT8},
	[TY_INT16 - K_TYPE] = {.kind = TY_INT16},
	[TY_INT32 - K_TYPE] = {.kind = TY_INT32},
	[TY_INT64 - K_TYPE] = {.kind = TY_INT64},
	[TY_UINT8 - K_TYPE] = {.kind = TY_UINT8},
	[TY_UINT16 - K_TYPE] = {.kind = TY_UINT16},
	[TY_UINT32 - K_TYPE] = {.kind = TY_UINT32},
	[TY_UINT64 - K_TYPE] = {.kind = TY_UINT64},
	[TY_BOOL - K_TYPE] = {.kind = TY_BOOL},
	[TY_STRING - K_TYPE] = {.kind = TY_STRING},
};

static uint64_t stock_type_count = sizeof(stock_types) / sizeof(Type);
static Type void_ptr_stock_type = {.kind = TY_PTR, .subtype = &stock_types[TY_VOID - K_TYPE]};

Token *new_token(Token value)
{
	Token *token = alloc(sizeof(Token));
	*token = value;
	return token;
}

Type *new_type(Type value)
{
	int stock_type_index = value.kind - K_TYPE;

	if(stock_type_index < stock_type_count)
		return &stock_types[stock_type_index];
	else if(value.kind == TY_PTR && value.subtype->kind == TY_VOID)
		return &void_ptr_stock_type;

	Type *type = alloc(sizeof(Type));
	*type = value;
	return type;
}

Expr *new_expr(Expr value)
{
	Expr *expr = alloc(sizeof(Expr));
	*expr = value;
	return expr;
}

Decl *new_decl(Decl value)
{
	Decl *decl = alloc(sizeof(Decl));
	*decl = value;
	return decl;
}

Stmt *new_stmt(Stmt value)
{
	Stmt *stmt = alloc(sizeof(Stmt));
	*stmt = value;
	return stmt;
}

Decl *lookup_flat_in(Token *uid, Scope *scope)
{
	array_for(scope->decls, i)
		if(scope->decls[i]->uid == uid)
			return scope->decls[i];

	return 0;
}

Decl *lookup_in(Token *uid, Scope *scope)
{
	Decl *decl = lookup_flat_in(uid, scope);

	if(decl)
		return decl;

	if(scope->parent)
		return lookup_in(uid, scope->parent);

	return 0;
}

int scope_contains_scope(Scope *upper, Scope *lower)
{
	if(lower->parent) {
		if(lower->parent == upper) return 1;
		return scope_contains_scope(upper, lower->parent);
	}

	return 0;
}

Block *create_block(Stmt **stmts, Scope *scope)
{
	Block *block = alloc(sizeof(Block));
	block->stmts = stmts;
	block->scope = scope;
	return block;
}

#endif