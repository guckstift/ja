
#include <stdlib.h>
#include "print.h"

#include "parse_utils.h"

static Stmt *p_stmts(Decl *func)
{
	ParseState state;
	pack_state(&state);
	Stmt *stmts = p_stmts_pub(&state, func);
	unpack_state(&state);
	return stmts;
}

Stmt *parse(Tokens *tokens, char *_unit_id)
{
	// save states
	Token *old_cur = cur;
	Token *old_last = last;
	char *old_src_end = src_end;
	Scope *old_scope = scope;
	char *old_unit_id = unit_id;
	
	cur = tokens->first;
	last = 0;
	src_end = tokens->last->start + tokens->last->length;
	scope = 0;
	unit_id = _unit_id;
	Stmt *stmts = p_stmts(0);
	if(!eat(TK_EOF))
		fatal_at(cur, "invalid statement");
	
	// restore states
	cur = old_cur;
	last = old_last;
	src_end = old_src_end;
	scope = old_scope;
	unit_id = old_unit_id;
	
	return stmts;
}
