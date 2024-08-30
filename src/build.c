#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "string.h"
#include "build.h"
#include "error.h"
#include "print.h"
#include "lex.h"
#include "parse.h"
#include "analyze.h"
#include "gen.h"
#include "arena.h"

#define HEXCHARS "0123456789abcdef"

static char *cachedir;

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

static Module *build_module(char *file)
{
	Module *module = alloc(sizeof(Module));

	module->srcfile = file;
	module->uid = idfy(module->srcfile);
	module->hfile = string_concat(cachedir, "/", module->uid, ".h", 0);
	module->cfile = string_concat(cachedir, "/", module->uid, ".c", 0);
	module->ofile = string_concat(cachedir, "/", module->uid, ".o", 0);

	module->src = load_file(module->srcfile);

	lex(module);

	#ifndef JA_TEST
	print_tokens_short(module->tokens);
	#endif

	parse(module);

	#ifndef JA_TEST
	print_ast(module->root);
	#endif

	if(had_errors())
		return 0;

	analyze(module);

	#ifndef JA_TEST
	print_ast(module->root);
	#endif

	if(had_errors())
		return 0;

	gen(module);

	#ifndef JA_TEST
	print_c_code(module->cfile);
	#endif

	if(had_errors())
		return 0;

	char *cmd = string_concat("gcc -c -o ", module->ofile, " ", module->cfile, 0);
	system(cmd);

	return module;
}

Project *build(char *mainfile)
{
	cachedir = string_concat(getenv("HOME"), "/.ja", 0);
	mkdir(cachedir, 0700);

	char *abs_mainfile = canonicalize_file_name(mainfile);

	if(abs_mainfile == 0)
		abs_mainfile = mainfile;

	Project *project = alloc(sizeof(Project));
	project->main = build_module(abs_mainfile);

	if(project->main == 0)
		return 0;

	project->progfile = string_concat(cachedir, "/", project->main->uid, 0);
	char *cmd = string_concat("gcc -o ", project->progfile, " ", project->main->ofile, 0);
	system(cmd);

	return project;
}