#include "parse.h"
#include "parse_impl.h"
#include "parse_stmt.h"

void parse(Module *module)
{
	setscope(0);
	setcur(module->tokens);
	module->root = parse_block(0);
}