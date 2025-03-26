#ifndef parse_H
#define parse_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define parse_C
#endif

#include "ast.c"

void parse(Module *module);

#endif
#ifdef parse_C

#include "parse_impl.c"
#include "parse_stmt.c"

void parse(Module *module)
{
	setscope(0);
	setcur(module->tokens);
	module->root = parse_block(0);
}

#endif