#include <stdlib.h>
#include "print.h"

#define match(t)  (cur->kind == (t))
#define adv()     (last = cur++)
#define eat(t)    (match(t) ? adv() : 0)
#define eatkw(k)  (cur->kind == TK_KEYWORD && cur->keyword == (k) ? adv() : 0)
#define eatpt(p)  (cur->kind == TK_PUNCT && cur->punct_id == (p) ? adv() : 0)

#define error(line, linep, start, ...) \
	print_error(line, linep, src_end, start, __VA_ARGS__)

#define error_at(token, ...) \
	error((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define error_after(token, ...) \
	error( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

#define fatal(line, linep, start, ...) do { \
	error(line, linep, start, __VA_ARGS__); \
	exit(EXIT_FAILURE); \
} while(0)

#define fatal_at(token, ...) \
	fatal((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define fatal_after(token, ...) \
	fatal( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

typedef struct {
	Token *cur;
	Token *last;
	char *src_end;
	Scope *scope;
	char *unit_id;
} ParseState;

static Token *cur;
static Token *last;
static char *src_end;
static Scope *scope;
static char *unit_id;

Expr *cast_expr(Expr *expr, Type *type, int explicit);
Expr *p_expr_pub(ParseState *state);

Block *p_block_pub(ParseState *state, Scope *scope);

Type *p_type_pub(ParseState *state);

static Decl *lookup_flat(Token *id)
{
	return lookup_flat_in(id, scope);
}

static Decl *lookup(Token *id)
{
	return lookup_in(id, scope);
}

static void unpack_state(ParseState *state)
{
	cur = state->cur;
	last = state->last;
	src_end = state->src_end;
	scope = state->scope;
	unit_id = state->unit_id;
}

static void pack_state(ParseState *state)
{
	state->cur = cur;
	state->last = last;
	state->src_end = src_end;
	state->scope = scope;
	state->unit_id = unit_id;
}
