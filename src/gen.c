#ifndef gen_H
#define gen_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define gen_C
#endif

#include "ast.c"

void gen(Module *module);

#endif
#ifdef gen_C

#include <stdio.h>
#include <stdarg.h>
#include "print.c"
#include "array.c"
#include "error.c"
#include "lex.c"
#include "gen_impl.c"
#include "gen_expr.c"
#include "gen_type.c"
#include "gen_stmt.c"

static int inheader;

static char *write_jaid(FILE *fs, char *msg, va_list args)
{
	Token *ident = va_arg(args, Token*);
	write("ja_");
	fwrite(ident->start, 1, ident->length, fs);
	return msg + 1;
}

static void gen_imports(Scope *scope)
{
	array_for(scope->imports, i) {
		Stmt *import = scope->imports[i];
		write("#include \"%s\"\n", import->module->hfile);
	}
}

static void gen_vardecls(Scope *scope)
{
	/*
	for(Decl *decl = scope->first; decl; decl = decl->next_decl)
		if(decl->stmt.kind == ST_VARDECL)
			gen_vardecl(decl);
	*/

	array_for(scope->decls, i) {
		Decl *decl = scope->decls[i];

		if(decl->stmt.kind == ST_VARDECL)
			gen_vardecl(decl);
	}

}

static void gen_funcdecls(Scope *scope)
{
	/*
	for(Decl *decl = scope->first; decl; decl = decl->next_decl)
		if(decl->stmt.kind == ST_FUNCDECL)
			gen_funcdecl(decl);
	*/

	array_for(scope->decls, i) {
		Decl *decl = scope->decls[i];

		if(decl->stmt.kind == ST_FUNCDECL)
			gen_funcdecl(decl);
	}
}

static void gen_mainfunchead(Module *module)
{
	write("int main_%s(int argc, char **argv)", module->uid);
}

static void gen_h(Module *module)
{
	set_fs(fopen(module->hfile, "w"));
	inheader = 1;

	write(
		"#ifndef HEADER_%s\n"
		"#define HEADER_%s\n"
		, module->uid, module->uid
	);

	gen_mainfunchead(module);

	write(
		";\n"
		"#endif"
	);

	close_fs();
}

static void gen_c(Module *module)
{
	set_fs(fopen(module->cfile, "w"));
	inheader = 0;

	write(
		"#include <inttypes.h>\n"
		"#include <stdint.h>\n"
		"#include <stdio.h>\n"
		"#include <string.h>\n"
		"#include \"runtime.h\"\n"
	);

	gen_imports(module->root->scope);
	gen_vardecls(module->root->scope);
	gen_funcdecls(module->root->scope);

	write("static int main_was_called = 0;\n");

	gen_mainfunchead(module);
	inclevel();

	write(
		" {\n"
		"%>if(main_was_called) return 0;\n"
		"%>else main_was_called = 1;\n"
	);

	declevel();
	gen_block(module->root);
	write("}\n");

	close_fs();
}

void gen(Module *module)
{
	register_escapemod('y', write_type_prefix);
	register_escapemod('z', write_type_postfix);
	register_escapemod('Y', write_full_type);
	register_escapemod('e', write_expr);
	register_escapemod('E', write_init_expr);
	register_escapemod('j', write_jaid);
	register_escapemod('S', write_string_literal);
	gen_h(module);
	gen_c(module);
	reset_escapemods();
}

#endif