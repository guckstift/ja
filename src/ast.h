#ifndef AST_H
#define AST_H

#include "lex.h"

typedef struct Unit Unit;
typedef struct Type Type;
typedef struct Expr Expr;
typedef struct StmtHead StmtHead;
typedef struct Import Import;
typedef struct DllImport DllImport;
typedef struct DeclFlags DeclFlags;
typedef struct Decl Decl;
typedef struct If If;
typedef struct While While;
typedef struct Assign Assign;
typedef struct Call Call;
typedef struct Print Print;
typedef struct Return Return;
typedef struct Stmt Stmt;
typedef struct Scope Scope;
typedef struct Block Block;
typedef struct ForEach ForEach;
typedef struct Delete Delete;

/*
	Kind
	
	enum for types, expressions and statements
*/

typedef enum {
	// primitive types
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
	CSTRING,
	
	_PRIMKIND_COUNT,
	
	// types
	PTR,
	ARRAY,
	FUNC,
	STRUCT,
	
	// expressions
	VAR,
	DEREF,
	CAST,
	SUBSCRIPT,
	BINOP,
	CALL,
	MEMBER,
	LENGTH,
	NEW,
	
	// statements
	PRINT,
	IF,
	WHILE,
	ASSIGN,
	RETURN,
	IMPORT,
	DLLIMPORT,
	BREAK,
	CONTINUE,
	FOREACH,
	DELETE,
} Kind;

/*
	Type
	
	* primitive type
	* ptr
	* array
	* func
	* struct
*/

struct Type {
	Kind kind;
	
	union {
		Type *subtype; // ptr target type
		Type *itemtype; // array item type
		Type *returntype; // func return type
	};
	
	union {
		int64_t length; // array length (-1 = incomplete)
		Decl *structdecl; // struct
		Type **paramtypes; // func
	};
};

Type *new_type(Kind kind);
Type *new_ptr_type(Type *subtype);
Type *new_array_type(int64_t length, Type *itemtype);
Type *new_dynarray_type(Type *itemtype);
Type *new_func_type(Type *returntype, Type **paramtypes);
Type *new_struct_type(Decl *decl);

int type_equ(Type *left, Type *right);
int is_integer_type(Type *type);
int is_integral_type(Type *type);
int is_complete_type(Type *type);
int is_dynarray_ptr_type(Type *type);

/*
	Expr
	
	centralized struct for any expression
*/

struct Expr {
	Kind kind;
	Token *start;
	Type *type;
	int isconst : 1;
	int islvalue : 1;
	
	union {
		int64_t value; // int, bool
		int64_t length; // string
		Decl *decl; // var
		Decl *member; // member
		Expr *subexpr; // ptr, cast
		Expr *ptr; // deref
		Expr *array; // subscript, length
		Expr *callee; // call
		Expr *left; // binop
		Expr **items; // array
	};
	
	union {
		char *string; // string, cstring
		Expr *right; // binop
		Expr *index; // subscript
		Expr *object; // member
		Expr **args; // call
	};
	
	Token *operator; // binop
};

Expr *new_expr(Kind kind, Token *start, Type *type, int isconst, int islvalue);
Expr *new_int_expr(Token *start, int64_t value);
Expr *new_bool_expr(Token *start, int64_t value);
Expr *new_string_expr(Token *start, char *string, int64_t length);
Expr *new_cstring_expr(Token *start, char *string);
Expr *new_var_expr(Token *start, Decl *decl);
Expr *new_array_expr(Token *start, Expr **items, int isconst);
Expr *new_subscript_expr(Expr *array, Expr *index);
Expr *new_length_expr(Expr *array);
Expr *new_cast_expr(Expr *subexpr, Type *type);
Expr *new_member_expr(Expr *object, Decl *member);
Expr *new_deref_expr(Token *start, Expr *ptr);
Expr *new_ptr_expr(Token *start, Expr *subexpr);
Expr *new_call_expr(Expr *callee, Expr **args);
Expr *new_binop_expr(Expr *left, Expr *right, Token *operator, Type *type);
Expr *new_new_expr(Token *start, Type *obj_type);

/*
	Statment Head
*/

#define STMT_HEAD \
	Kind kind; \
	Token *start; \
	Scope *scope; \

/*
	Decl
*/

struct Decl {
	STMT_HEAD
	Token *id;
	char *private_id;
	char *public_id;
	Type *type;
	
	uint8_t imported;
	uint8_t exported;
	uint8_t builtin;
	uint8_t isproto;
	uint8_t cfunc;
	
	union {
		Expr *init; // var
		Block *body; // func
	};
	
	union {
		Decl **members; // struct
		Decl **params; // func
	};
};

Decl *new_decl(
	Kind kind, Token *start, Scope *scope, Token *id, int exported,
	Type *type
);

Decl *new_var(
	Token *start, Scope *scope, Token *id, int exported, Type *type,
	Expr *init
);

Decl *new_func(
	Token *start, Scope *scope, Token *id, int exported, Type *returntype,
	Decl **params
);

Decl *new_struct(
	Token *start, Scope *scope, Token *id, int exported, Decl **members
);

Decl *clone_decl(Decl *decl);

/*
	Stmt
*/

struct Import {
	STMT_HEAD
	Unit *unit;
	Decl **decls;
};

struct DllImport {
	STMT_HEAD
	char *dll_name;
	Decl **decls;
};

struct If {
	STMT_HEAD
	Expr *cond;
	Block *if_body;
	Block *else_body;
};

struct While {
	STMT_HEAD
	Expr *cond;
	Block *body;
};

struct Assign {
	STMT_HEAD
	Expr *target;
	Expr *expr;
};

struct Call {
	STMT_HEAD
	Expr *call;
};

struct Print {
	STMT_HEAD
	Expr *expr;
};

struct Return {
	STMT_HEAD
	Expr *expr;
};

struct ForEach {
	STMT_HEAD
	Expr *array;
	Decl *iter;
	Block *body;
};

struct Delete {
	STMT_HEAD
	Expr *expr;
};

struct Stmt {
	union {
		struct { STMT_HEAD };
		Decl as_decl;
		Import as_import;
		DllImport as_dll_import;
		If as_if;
		While as_while;
		Assign as_assign;
		Call as_call;
		Print as_print;
		Return as_return;
		ForEach as_foreach;
		Delete as_delete;
	};
};

Stmt *new_stmt(Kind kind, Token *start, Scope *scope);
Import *new_import(Token *start, Scope *scope, Unit *unit, Decl **decls);
DllImport *new_dll_import(Token *start, Scope *scope, char *name, Decl **decls);
If *new_if(Token *start, Expr *cond, Block *if_body, Block *else_body);
While *new_while(Token *start, Scope *scope, Expr *cond, Block *body);
Assign *new_assign(Scope *scope, Expr *target, Expr *expr);
Call *new_call(Scope *scope, Expr *call);
Print *new_print(Token *start, Scope *scope, Expr *expr);
Return *new_return(Token *start, Scope *scope, Expr *expr);
Delete *new_delete(Token *start, Scope *scope, Expr *expr);

ForEach *new_foreach(
	Token *start, Scope *scope, Expr *array, Decl *iter, Block *body
);

struct Scope {
	char *unit_id;
	Scope *parent;
	Decl *funchost;
	Decl *structhost;
	Stmt *loophost;
	Import **imports;
	DllImport **dll_imports;
	Decl **decls;
};

Scope *new_scope(char *unit_id, Scope *parent);

Decl *lookup_flat_in(Token *id, Scope *scope);
Decl *lookup_in(Token *id, Scope *scope);
int declare_in(Decl *decl, Scope *scope);
int redeclare_in(Decl *decl, Scope *scope);

struct Block {
	Stmt **stmts;
	Scope *scope;
};

Block *new_block(Stmt **stmts, Scope *scope);

#endif
