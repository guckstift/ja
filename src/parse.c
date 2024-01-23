#include <stdio.h>
#include <stdlib.h>
#include "parse_internal.h"
#include "print.h"
#include "array.h"

static Block *p_block(Scope *scope)
{
	ParseState state;
	pack_state(&state);
	Block *block = p_block_pub(&state, scope);
	unpack_state(&state);
	return block;
}

Block *parse(Token *tokens, char *_unit_id)
{
	// save states
	ParseState old_state;
	pack_state(&old_state);

	Token *last_token = tokens + array_length(tokens) - 1;

	cur = tokens;
	last = 0;
	src_end = last_token->start + last_token->length;
	scope = 0;
	unit_id = _unit_id;

	Block *block = p_block(0);

	if(!eat(TK_EOF))
		fatal_at(cur, "invalid statement");

	// restore states
	unpack_state(&old_state);

	return block;
}
