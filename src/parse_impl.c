#ifndef parse_impl_H
#define parse_impl_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define parse_impl_C
#endif

#include <stdbool.h>
#include "ast.c"
#include "error.c"

#define enter        parse_enter
#define leave        parse_leave
#define setscope     parse_setscope
#define setcur       parse_setcur
#define peek         parse_peek
#define advance      parse_advance
#define getlast      parse_last
#define expect       parse_expect

#define error_at_cur(...)  error_at(peek(), __VA_ARGS__)

#define match(k)  (peek()->kind == k)
#define eat(k)    (match(k) ? advance() : 0)

extern int e_before;

Scope *getscope();
void setscope(Scope *new_scope);
void enter();
Scope *leave();
Scope *touchscope();
void scope_add_decl(Decl *decl);
Token *peek();
void *setcur(Token *pos);
Token *advance();
Token *getlast();
void *expect(Kind kind, char *msg);

#endif
#ifdef parse_impl_C

#include "parse_stmt.c"
#include "parse_expr.c"
#include "parse_type.c"
#include "array.c"
#include "arena.c"

int e_before = 0;

static Scope *scope;
static Token *cur;
static Token *last;

Scope *getscope()
{
	return scope;
}

void setscope(Scope *new_scope)
{
	scope = new_scope;
}

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

Scope *touchscope()
{
	enter();
	return leave();
}

Token *peek()
{
	return cur;
}

void *setcur(Token *pos)
{
	cur = pos;
}

void scope_add_decl(Decl *decl)
{
	if(scope->first) scope->last = scope->last->next = decl;
	else scope->last = scope->first = decl;
	decl->scope = scope;
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
					error("expected identifier but %n is a reserved keyword", cur);
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

#endif