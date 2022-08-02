#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <libgen.h>
#include "build.h"
#include "print.h"
#include "analyze.h"
#include "cgen.h"
#include "string.h"
#include "../build/runtime.h.res"
#include "../build/runtime.c.res"

#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

#define lo_nibble(x) ((x) & 0xf)
#define hi_nibble(x) ((x) >> 4 & 0xf)

static char *cache_dir;
static char *cur_unit_dirname;
static Project *project;
static BuildOptions options;

static void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(0, 0, 0, 0, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static char *idfy(char *input)
{
	uint64_t len = strlen(input);
	uint64_t olen = len * 4 + 1 + 2;
	char *output = malloc(olen);
	char *ip = input;
	char *op = output;
	
	while(*ip) {
		if(*ip <= 0x1f) {
			// control chars and out-of-ascii
			*op++ = '_';
			uint8_t byte = *ip;
			if(byte > 100) *op++ = byte / 100 + '0';
			if(byte > 10) *op++ = byte / 10 % 10 + '0';
			*op++ = byte % 10 + '0';
		}
		else if(isalnum(*ip)) {
			// digits
			*op++ = *ip;
		}
		else if(*ip == '_') {
			// underscore
			*op++ = '_';
			*op++ = '_';
		}
		else if(*ip <= '/') {
			// space and first puncts (a-p)
			*op++ = '_';
			*op++ = *ip - ' ' + 'a';
		}
		else if(*ip <= '@') {
			// second puncts (q-w)
			*op++ = '_';
			*op++ = *ip - ':' + 'q';
		}
		else if(*ip <= '`') {
			// third puncts (A-F)
			*op++ = '_';
			*op++ = *ip - '[' + 'A';
		}
		else if(*ip <= 0x7f) {
			// last puncts (G-K)
			*op++ = '_';
			*op++ = *ip - '{' + 'G';
		}
		ip++;
	}
	*op++ = '_';
	*op++ = 'X';
	*op = 0;
	return output;
}

static int run_cmd(char *cmd)
{
	#ifdef JA_DEBUG
	printf(COL_YELLOW "[run]:" COL_RESET " %s\n", cmd);
	#endif
	return system(cmd);
}

static void compile_c(char *cfile, char *ofile)
{
	char *cmd = string_concat(
		"gcc -c -std=c17 -pedantic-errors -o ", ofile, " ", cfile, 0
	);
	
	int res = run_cmd(cmd);
	if(res) error("could not compile %s", cfile);
}

static int dir_exists(char *dirname)
{
	DIR *dir = opendir(dirname);
	if(dir == 0) return errno == EACCES;
	closedir(dir);
	return 1;
}

static Unit *build_unit(char *filename, int ismain)
{
	#ifdef JA_DEBUG
	printf(COL_YELLOW "=== building unit %s ===" COL_RESET "\n",filename);
	#endif
	
	array_for(project->units, i) {
		Unit *unit = project->units[i];
		
		if(strcmp(unit->src_filename, filename) == 0) {
			return unit;
		}
	}
	
	char *old_unit_dirname = cur_unit_dirname;
	cur_unit_dirname = string_clone(filename);
	cur_unit_dirname = dirname(cur_unit_dirname);
	
	Unit *unit = malloc(sizeof(Unit));
	unit->ismain = ismain;
	unit->src_filename = filename;
	unit->unit_id = idfy(unit->src_filename);
	
	unit->c_filename = string_concat(cache_dir, "/", unit->unit_id, 0);
	unit->h_filename = string_concat(unit->c_filename, ".h", 0);
	unit->c_main_filename = string_concat(unit->c_filename, ".main.c", 0);
	string_append(unit->c_filename, ".c");
	
	FILE *fs = fopen(filename, "rb");
	if(!fs) error("can not open input file '%s'", filename);
	fseek(fs, 0, SEEK_END);
	unit->src_len = ftell(fs);
	rewind(fs);
	unit->src = malloc(unit->src_len + 1);
	unit->src[unit->src_len] = 0;
	fread(unit->src, 1, unit->src_len, fs);
	fclose(fs);
	
	array_push(project->units, unit);
	
	#ifdef JA_DEBUG
		printf(COL_YELLOW "=== lexing ===" COL_RESET "\n");
	#endif
	
	unit->tokens = lex(unit->src, unit->src_len);
	
	if(options.show_tokens)
		print_tokens(unit->tokens);
	
	#ifdef JA_DEBUG
		printf(COL_YELLOW "=== parsing ===" COL_RESET "\n");
	#endif
	
	unit->block = parse(unit->tokens, unit->unit_id);
	
	#ifdef JA_DEBUG
		printf(COL_YELLOW "[OK]" COL_RESET "\n");
	#endif
	
	if(options.show_ast)
		print_ast(unit->block);
	
	#ifdef JA_DEBUG
		printf(COL_YELLOW "=== analyzing ===" COL_RESET "\n");
	#endif
	
	analyze(unit);
	
	#ifdef JA_DEBUG
		printf(COL_YELLOW "[OK]" COL_RESET "\n");
	#endif
	
	if(options.show_ast)
		print_ast(unit->block);
	
	#ifdef JA_DEBUG
	printf(COL_YELLOW "=== generating code ===" COL_RESET "\n");
	#endif
	gen(unit);
	#ifdef JA_DEBUG
	print_c_code(unit->c_filename);
	#endif
	
	unit->obj_filename = string_concat(cache_dir, "/", unit->unit_id, 0);
	unit->obj_main_filename = string_concat(unit->obj_filename, ".main.o", 0);
	string_append(unit->obj_filename, ".o");
	
	compile_c(unit->c_filename, unit->obj_filename);
	
	if(unit->ismain) {
		compile_c(unit->c_main_filename, unit->obj_main_filename);
	}
	
	#ifdef JA_DEBUG
	printf(COL_YELLOW "=== compiled unit %s ===" COL_RESET "\n", filename);
	#endif
	
	cur_unit_dirname = old_unit_dirname;
	return unit;
}

Unit *import(char *filename)
{
	char *abs_filename = string_concat(cur_unit_dirname, "/", filename, 0);
	char *real_filename = realpath(abs_filename, NULL);
	
	if(real_filename == 0) {
		error("could not import %s\n", abs_filename);
	}
	
	return build_unit(real_filename, 0);
}

void write_cache_file(char *name, char *text)
{
	char *path = string_concat(cache_dir, "/", name, 0);
	FILE *fs = fopen(path, "wb");
	fwrite(text, 1, strlen(text), fs);
	fclose(fs);
}

Project *build(BuildOptions _options)
{
	options = _options;
	
	project = malloc(sizeof(Project));
	project->units = 0;
	
	cache_dir = string_concat(getenv("HOME"), "/.ja", 0);
	
	if(!dir_exists(cache_dir)) {
		mkdir(cache_dir, 0755);
	}
	
	write_cache_file("runtime.h", RUNTIME_H_RES);
	write_cache_file("runtime.c", RUNTIME_C_RES);
	
	char *real_main_filename = realpath(options.main_filename, NULL);
	Unit *main_unit = build_unit(real_main_filename, 1);
	
	#ifdef JA_DEBUG
	printf(COL_YELLOW "=== linking ===" COL_RESET "\n");
	#endif
	
	if(!options.outfilename) {
		options.outfilename = string_concat(
			cache_dir, "/", main_unit->unit_id, 0
		);
	}
	
	char *cmd = string_concat(
		"gcc -o ", options.outfilename, " ", cache_dir, "/runtime.c", 0
	);
	
	array_for(project->units, i) {
		Unit *unit = project->units[i];
		string_append(cmd, " ");
		string_append(cmd, unit->obj_filename);
		
		if(unit->ismain) {
			string_append(cmd, " ");
			string_append(cmd, unit->obj_main_filename);
		}
	}
	
	string_append(cmd, " -ldl");
	
	int res = run_cmd(cmd);
	if(res) error("could not link the object files");
	
	project->exe_filename = options.outfilename;
	printf(COL_YELLOW "=== done ===" COL_RESET "\n");
	return project;
}
