#include "parse_impl.h"
#include "parse_stmt.h"
#include "parse_expr.h"
#include "parse_type.h"
#include "array.h"
#include "string.h"
#include "arena.h"

int e_before = 0;

static Scope *scope;
static Token *cur;
static Token *last;

void enter()
{
	Scope *new_scope = alloc(sizeof(Scope));
	new_scope->parent = scope;
	scope = new_scope;
}

Scope *leave()
{
	Scope *old_scope = scope;
	scope = scope->parent;
	return old_scope;
}

void setscope(Scope *new_scope)
{
	scope = new_scope;
}

void *setcur(Token *pos)
{
	cur = pos;
}

bool declare_in(Stmt *decl, Scope *scope)
{
	if(lookup_flat_in(decl->id, scope))
		return false;

	array_push(scope->decls, decl);
	return true;
}

bool declare(Stmt *decl)
{
	return declare_in(decl, scope);
}

Token *peek()
{
	return cur;
}

Token *advance()
{
	return last = cur ++;
}

Token *getlast()
{
	return last;
}

void *expect(Kind kind, char *msg)
{
	void *result = 0;

	if(!error_tok)
		error_tok = cur;

	switch(kind) {
		case K_STMT:
			result = parse_stmt();

			if(!result)
				error(msg ? msg : "expected statement");

			break;

		case K_EXPR:
			result = parse_expr();

			if(!result)
				error(msg ? msg : "expected expression");

			break;

		case K_TYPE:
			result = parse_type();

			if(!result)
				error(msg ? msg : "expected type");

			break;

		default:
			result = eat(kind);

			if(!result) {
				if(msg) {
					error(msg);
				}
				else if(kind == TK_IDENT && cur->kind >= TK_KEYWORDS_START && cur->kind < TK_PUNCTS_START) {
					error("expected identifier but %t is a reserved keyword", cur);
					cur ++;
				}
				else {
					error("expected %s", token_names[kind]);
				}
			}
	}

	error_reset_opts();
	return result;
}