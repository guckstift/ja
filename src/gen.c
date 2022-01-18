#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "gen.h"

static FILE *ofs;
static int64_t level;

static void write(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vfprintf(ofs, msg, args);
	va_end(args);
}

static void gen_indent()
{
	for(int64_t i=0; i<level; i++) write("    ");
}

static void gen_ident(Token *id)
{
	write("ja_");
	fwrite(id->start, 1, id->length, ofs);
}

static void gen_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			write("INT64_C(%" PRId64 ")", expr->ival);
			break;
		case EX_VAR:
			gen_ident(expr->id);
			break;
	}
}

static void gen_stmt(Stmt *stmt)
{
	switch(stmt->type) {
		case ST_PRINT:
			gen_indent();
			write("printf(\"%\" PRId64 \"\\n\", ");
			gen_expr(stmt->expr);
			write(");\n");
			break;
	}
}

static void gen_stmts(Stmt *stmts)
{
	level ++;
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		gen_stmt(stmt);
	}
	level --;
}

static void gen_vardecl(Stmt *stmt)
{
	write("int64_t ");
	gen_ident(stmt->id);
	write(" = INT64_C(0);\n");
}

static void gen_vardecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->type == ST_VARDECL) {
			gen_vardecl(stmt);
		}
	}
}

void gen(Unit *unit)
{
	ofs = stdout;
	ofs = fopen("./output.c", "wb");
	level = 0;
	write("#include <stdio.h>\n");
	write("#include <stdint.h>\n");
	write("#include <inttypes.h>\n");
	gen_vardecls(unit->stmts);
	write("int main(int argc, char **argv) {\n");
	gen_stmts(unit->stmts);
	write("}\n");
}
