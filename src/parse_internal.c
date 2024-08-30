#include "parse_internal.h"

Token *cur;
Token *last;
char *src_end;
Scope *scope;
char *unit_id;

Decl *lookup_flat(Token *id)
{
	return lookup_flat_in(id, scope);
}

Decl *lookup(Token *id)
{
	return lookup_in(id, scope);
}

void unpack_state(ParseState *state)
{
	cur = state->cur;
	last = state->last;
	src_end = state->src_end;
	scope = state->scope;
	unit_id = state->unit_id;
}

void pack_state(ParseState *state)
{
	state->cur = cur;
	state->last = last;
	state->src_end = src_end;
	state->scope = scope;
	state->unit_id = unit_id;
}