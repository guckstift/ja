#ifndef build_H
#define build_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define build_C
#endif

#define _GNU_SOURCE

#include "ast.c"

Module *import_module(char *filename, char *cur_dir);
Project *build(char *mainfile);

#endif
#ifdef build_C

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "string.c"
#include "error.c"
#include "print.c"
#include "lex.c"
#include "parse.c"
#include "analyze.c"
#include "gen.c"
#include "arena.c"

static char runtime_h_src[] = {
	#include "build/runtime.h.res"
	,0};
static char runtime_c_src[] = {
	#include "build/runtime.c.res"
	,0};

static char *cachedir;
static Project *project;

static char *idfy(char *input)
{
	char *output = alloc(strlen(input) * 3 + 1);

	for(char *ip = input, *op = output; *ip; ip ++) {
		if(ip > input && isalnum(*ip)) {
			*op++ = *ip;
		}
		else if(*ip == '_') {
			*op++ = '_';
			*op++ = '_';
		}
		else {
			*op++ = '_';
			*op++ = HEXCHARS[*ip >> 4];
			*op++ = HEXCHARS[*ip & 15];
		}
	}

	return output;
}

static char *load_file(char *file)
{
	FILE *fs = fopen(file, "r");

	if(fs == 0) {
		error("could not open file '%s'", file);
		return "";
	}

	fseek(fs, 0, SEEK_END);
	long length = ftell(fs);
	rewind(fs);
	char *text = alloc(length + 1);
	fread(text, 1, length, fs);
	text[length] = 0;

	for(uint64_t i=0; i<length; i++) {
		if(text[i] == 0) {
			error("zero byte in source file '%s' at position %u", file, i);
			text[i] = ' ';
		}
	}

	return text;
}

static char *store_cache_file(char *filename, char *contents)
{
	char *path = string_concat(cachedir, "/", filename, 0);
	FILE *fs = fopen(path, "wb");
	fwrite(contents, 1, strlen(contents), fs);
	fclose(fs);
	return path;
}

static int run_cmd(char *cmd)
{
	#ifdef JA_DEBUG
	debug_print(COL_YELLOW "[run]:" COL_RESET " %s\n", cmd);
	#endif
	return system(cmd);
}

static int compile_c(char *cfile, char *ofile)
{
	char *cmd = string_concat("gcc -c -pedantic-errors -o ", ofile, " ", cfile, 0);
	int res = run_cmd(cmd);
	if(res) error("could not compile %s", cfile);
	return res == 0;
}

static void build_main_o(Module *main)
{
	char *main_c = string_concat(cachedir, "/main_", main->uid, ".c", 0);
	FILE *fs = fopen(main_c, "wb");
	fprint(fs, "int main_%s(int argc, char **argv);\n", main->uid);
	fprint(fs, "int main(int argc, char **argv) {\n");
	fprint(fs, "    main_%s(argc, argv);\n", main->uid);
	fprint(fs, "}\n");
	fclose(fs);
	main->maino = string_concat(cachedir, "/main_", main->uid, ".o", 0);
	compile_c(main_c, main->maino);
}

static void generate_paths(Module *module, char *abs_src_path)
{
	module->srcfile = abs_src_path;
	module->uid = idfy(module->srcfile);
	module->hfile = string_concat(cachedir, "/", module->uid, ".h", 0);
	module->cfile = string_concat(cachedir, "/", module->uid, ".c", 0);
	module->ofile = string_concat(cachedir, "/", module->uid, ".o", 0);

	for(char *c = module->srcfile, *sep = 0; *c; c++) {
		if(*c == '/') sep = c;
		if(c[1] == 0) {
			module->srcdir = string_prefix(module->srcfile, sep - module->srcfile);
			break;
		}
	}
}

static Module *build_module(char *abs_src_path)
{
	Module *module = alloc(sizeof(Module));
	generate_paths(module, abs_src_path);
	array_push(project->modules, module);

	module->src = load_file(module->srcfile);

	debug_print(COL_YELLOW "=== lexing" COL_RESET "\n");
	lex(module);
	debug_print(COL_YELLOW "... done" COL_RESET "\n");

	#ifndef JA_TEST
	print_tokens_short(module->tokens);
	#endif

	debug_print(COL_YELLOW "=== parsing" COL_RESET "\n");
	parse(module);
	debug_print(COL_YELLOW "... done" COL_RESET "\n");

	#ifndef JA_TEST
	print_ast(module->root);
	#endif

	if(had_errors())
		return 0;

	debug_print(COL_YELLOW "=== analyzing" COL_RESET "\n");
	analyze(module);
	debug_print(COL_YELLOW "... done" COL_RESET "\n");

	#ifndef JA_TEST
	print_ast(module->root);
	#endif

	if(had_errors())
		return 0;

	debug_print(COL_YELLOW "=== generating" COL_RESET "\n");
	gen(module);
	debug_print(COL_YELLOW "... done" COL_RESET "\n");

	#ifndef JA_TEST
	print_c_code(module->cfile);
	#endif

	if(had_errors())
		return 0;

	debug_print(COL_YELLOW "=== compiling" COL_RESET "\n");
	if(!compile_c(module->cfile, module->ofile))
		return 0;

	return module;
}

Module *import_module(char *filename, char *cur_dir)
{
	char *file_path = cur_dir
		? string_concat(cur_dir, "/", filename, 0)
		: string_concat("./", filename, 0);

	char *abs_src_path = canonicalize_file_name(file_path);

	array_for(project->modules, i) {
		Module *other = project->modules[i];
		if(strcmp(other->srcfile, abs_src_path) == 0) return other;
	}

	return build_module(abs_src_path);
}

Project *build(char *mainfile)
{
	cachedir = string_concat(getenv("HOME"), "/.ja", 0);
	mkdir(cachedir, 0700);

	char *runtime_h = store_cache_file("runtime.h", runtime_h_src);
	char *runtime_c = store_cache_file("runtime.c", runtime_c_src);
	char *runtime_o = string_concat(cachedir, "/runtime.o", 0);
	compile_c(runtime_c, runtime_o);

	project = alloc(sizeof(Project));
	project->main = import_module(mainfile, 0);

	if(project->main == 0)
		return 0;

	build_main_o(project->main);

	project->progfile = string_concat(cachedir, "/", project->main->uid, 0);

	debug_print(COL_YELLOW "=== linking" COL_RESET "\n");

	char *cmd = string_concat(
		"gcc -o ", project->progfile, " ", runtime_o, " ", project->main->maino, 0
	);

	array_for(project->modules, i) {
		string_append(cmd, " ");
		string_append(cmd, project->modules[i]->ofile);
	}

	if(run_cmd(cmd)) {
		error("could not link program");
		return 0;
	}

	return project;
}

#endif