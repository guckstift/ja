#include <stdio.h>
#include <stdarg.h>
#include "gen.h"
#include "print.h"
#include "array.h"
#include "ast.h"
#include "error.h"
#include "lex.h"
#include "gen_impl.h"
#include "gen_expr.h"
#include "gen_type.h"
#include "gen_stmt.h"

static int inheader;

static char *write_jaid(FILE *fs, char *msg, va_list args)
{
	Token *ident = va_arg(args, Token*);
	write("ja_");
	fwrite(ident->start, 1, ident->length, fs);
	return msg + 1;
}

static void gen_funcdecl(Stmt *decl)
{
	write("void %j() {\n", decl->id);
	gen_block(decl->body);
	write("}\n");
}

static void gen_vardecls(Scope *scope)
{
	array_for(scope->decls, i) {
		Stmt *decl = scope->decls[i];

		if(decl->kind == ST_VARDECL)
			gen_vardecl(decl);
	}
}

static void gen_funcdecls(Scope *scope)
{
	array_for(scope->decls, i) {
		Stmt *decl = scope->decls[i];

		if(decl->kind == ST_FUNCDECL)
			gen_funcdecl(decl);
	}
}

static void gen_h(Module *module)
{
	set_fs(fopen(module->hfile, "w"));
	inheader = 1;
	close_fs();
}

static void gen_c(Module *module)
{
	set_fs(fopen(module->cfile, "w"));
	inheader = 0;

	write("#include <inttypes.h>\n");
	write("#include <stdint.h>\n");
	write("#include <stdio.h>\n");
	write("#include <string.h>\n");

	gen_vardecls(module->root->scope);
	gen_funcdecls(module->root->scope);

	write("int main(int argc, char **argv) {\n");
	gen_block(module->root);
	write("}\n");

	close_fs();
}

void gen(Module *module)
{
	register_escapemod('y', write_type);
	register_escapemod('z', write_type_postfix);
	register_escapemod('Y', write_full_type);
	register_escapemod('e', write_expr);
	register_escapemod('j', write_jaid);
	gen_h(module);
	gen_c(module);
	reset_escapemods();
}