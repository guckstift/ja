#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "lex.h"

typedef struct Unit Unit;
typedef struct Type Type;
typedef struct Expr Expr;
typedef struct StmtHead StmtHead;
typedef struct Import Import;
typedef struct Foreign Foreign;
typedef struct EnumItem EnumItem;
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
typedef struct For For;
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
	SLICE,
	FUNC,
	STRUCT,
	ENUM,
	UNION,
	NAMED,
	
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
	NEGATION,
	COMPLEMENT,
	
	// statements
	PRINT,
	IF,
	WHILE,
	ASSIGN,
	RETURN,
	IMPORT,
	FOREIGN,
	BREAK,
	CONTINUE,
	FOR,
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
	* enum
	* union
*/

struct Type {
	Kind kind;
	
	union {
		Type *subtype; // ptr target type
		Type *itemtype; // array/slice item type
		Type *returntype; // func return type
		Token *id; // named type
	};
	
	union {
		int64_t length; // array length
		Decl *decl; // struct, enum, union
		Type **paramtypes; // func
	};
};

Type *new_type(Kind kind);
Type *new_ptr_type(Type *subtype);
Type *new_array_type(int64_t length, Type *itemtype);
Type *new_slice_type(Type *itemtype);
Type *new_func_type(Type *returntype, Type **paramtypes);
Type *new_struct_type(Decl *decl);
Type *new_enum_type(Decl *decl);
Type *new_union_type(Decl *decl);
Type *new_named_type(Token *id);

int type_equ(Type *left, Type *right);
int is_integer_type(Type *type);
int is_integral_type(Type *type);
bool is_array_ptr_type(Type *type);

/*
	Expr
	
	centralized struct for any expression
*/

typedef enum {
	OL_OR,
	OL_AND,
	OL_CMP,
	OL_ADD,
	OL_MUL,
	
	_OPLEVEL_COUNT,
} OpLevel;

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
		Expr *subexpr; // ptr, cast, negation
		Expr *ptr; // deref
		Expr *array; // subscript, length
		Expr *callee; // call
		Expr *left; // binop
		Expr **items; // array
	};
	
	union {
		Token *id; // var
		char *string; // string, cstring
		Expr *right; // binop
		Expr *index; // subscript
		Expr *object; // member
		Expr **args; // call
		EnumItem *item; // enum
	};
	
	union {
		Token *operator; // binop
		Token *member_id; // member (before analyze)
	};
	
	OpLevel oplevel; // binop
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
Expr *new_member_expr(Expr *object, Token *member_id);
Expr *new_deref_expr(Token *start, Expr *ptr);
Expr *new_ptr_expr(Token *start, Expr *subexpr);
Expr *new_call_expr(Expr *callee, Expr **args);
Expr *new_binop_expr(Expr *left, Expr *right, Token *operator, OpLevel oplevel);
Expr *new_new_expr(Token *start, Type *obj_type);
Expr *new_enum_item_expr(Token *start, Decl *enumdecl, EnumItem *item);

/*
	Statment Head
*/

#define STMT_HEAD \
	Kind kind; \
	Token *start; \
	Token *end; \
	Scope *scope; \

/*
	Decl
*/

struct EnumItem {
	Token *id;
	Expr *val;
	Decl *enumdecl;
};

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
	uint8_t deps_scanned;
	
	Decl **deps; // func: variables used from outer scope
	Scope *func_scope; // func
	
	union {
		Expr *init; // var
		Block *body; // func
	};
	
	union {
		Decl **members; // struct
		Decl **params; // func
		EnumItem **items; // enum
		uint8_t isparam; // var
	};
};

Decl *new_decl(
	Kind kind, Token *start, Scope *scope, Token *id, int exported,
	Type *type
);

Decl *new_var(
	Token *start, Scope *scope, Token *id, int exported, int isparam,
	Type *type, Expr *init
);

Decl *new_func(
	Token *start, Scope *scope, Token *id, int exported, Type *returntype,
	Decl **params, Scope *func_scope
);

Decl *new_struct(
	Token *start, Scope *scope, Token *id, int exported, Decl **members
);

Decl *new_enum(
	Token *start, Scope *scope, Token *id, EnumItem **items, int exported
);

Decl *new_union(
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

struct Foreign {
	STMT_HEAD
	char *filename;
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

struct For {
	STMT_HEAD
	Decl *iter;
	Expr *from;
	Expr *to;
	Block *body;
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
		Foreign as_foreign;
		If as_if;
		While as_while;
		Assign as_assign;
		Call as_call;
		Print as_print;
		Return as_return;
		For as_for;
		ForEach as_foreach;
		Delete as_delete;
	};
};

Stmt *new_stmt(Kind kind, Token *start, Scope *scope);
Import *new_import(Token *start, Scope *scope, Unit *unit, Decl **decls);
Foreign *new_foreign(Token *start, Scope *scope, char *name, Decl **decls);
If *new_if(Token *start, Expr *cond, Block *if_body, Block *else_body);
While *new_while(Token *start, Scope *scope, Expr *cond, Block *body);
Assign *new_assign(Scope *scope, Expr *target, Expr *expr);
Call *new_call(Scope *scope, Expr *call);
Print *new_print(Token *start, Scope *scope, Expr *expr);
Return *new_return(Token *start, Scope *scope, Expr *expr);
Delete *new_delete(Token *start, Scope *scope, Expr *expr);

For *new_for(
	Token *start, Scope *scope, Decl *iter, Expr *from, Expr *to, Block *body
);

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
	Foreign **foreigns;
	Decl **decls;
};

Scope *new_scope(char *unit_id, Scope *parent);

Decl *lookup_flat_in(Token *id, Scope *scope);
Decl *lookup_in(Token *id, Scope *scope);
bool scope_contains_scope(Scope *upper, Scope *lower);
int declare_in(Decl *decl, Scope *scope);
int redeclare_in(Decl *decl, Scope *scope);

struct Block {
	Stmt **stmts;
	Scope *scope;
};

Block *new_block(Stmt **stmts, Scope *scope);

#endif
