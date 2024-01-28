#include <stdio.h>
#include <stdlib.h>
#include "parse_internal.h"
#include "print.h"
#include "utils/array.h"

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

	Block *block = p_(block, 0);

	if(!eat(TK_EOF))
		fatal_at(cur, "invalid statement");

	// restore states
	unpack_state(&old_state);

	return block;
}
