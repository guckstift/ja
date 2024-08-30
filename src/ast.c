#include <stdio.h>
#include "ast.h"
#include "array.h"
#include "arena.h"
#include "string.h"
#include "lex.h"

char *token_names[] = {
	"end of file",
	"identifier",
	"integer literal",

	"<TK_KEYWORDS_START>",
	#define _(x) "keyword " #x,
	KEYWORDS
	#undef _

	"<TK_PUNCTS_START>",
	#define _(x, y) "'" x "'",
	PUNCTS
	#undef _
};

Token *new_token(Token value)
{
	Token *token = alloc(sizeof(Token));
	*token = value;
	return token;
}

Type *new_type(Type value)
{
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

Expr *expr_from_token(Token *token)
{
	if(!token)
		return 0;

	switch(token->kind) {
		case TK_INT:
			return create_int_expr(token->ival, .start = token);
		case KW_true: case KW_false:
			return create_bool_expr(token->kind == KW_true, .start = token);
		case KW_null:
			return create_null_expr(.start = token);
		case TK_IDENT:
			return create_var_expr(token->id, .start = token);
	}

	return 0;
}

Stmt *new_stmt(Stmt value)
{
	Stmt *stmt = alloc(sizeof(Stmt));
	*stmt = value;
	return stmt;
}

Stmt *lookup_flat_in(Token *id, Scope *scope)
{
	array_for(scope->decls, i)
		if(scope->decls[i]->id == id)
			return scope->decls[i];

	return 0;
}

Stmt *lookup_in(Token *id, Scope *scope)
{
	Stmt *decl = lookup_flat_in(id, scope);

	if(decl)
		return decl;

	if(scope->parent)
		return lookup_in(id, scope->parent);

	return 0;
}

char *create_jaid(Token *id)
{
	char *jaid = string_from_cstr("ja_");
	string_append_token(jaid, id);
	return jaid;
}

Expr *mock_int_var_expr()
{
	Type *type = create_type(TY_INT);
	Token *id = mock_id();
	Stmt *decl = create_vardecl(id, type, create_int_expr(0));
	Expr *var = create_var_expr(id, .decl = decl);
	return var;
}