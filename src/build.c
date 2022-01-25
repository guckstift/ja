#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include "build.h"
#include "print.h"
#include "gen.h"

#define lo_nibble(x) ((x) & 0xf)
#define hi_nibble(x) ((x) >> 4 & 0xf)

#define str_append(dest, src) (dest) = _str_append(dest, src)

static void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(0, 0, 0, 0, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static char *_str_append(char *dest, char *app)
{
	uint64_t newlen = (dest ? strlen(dest) : 0) + strlen(app);
	if(dest) {
		dest = realloc(dest, newlen + 1);
		strcat(dest, app);
	}
	else {
		dest = malloc(newlen + 1);
		strcpy(dest, app);
	}
	return dest;
}

static char *idfy(char *input)
{
	uint64_t len = strlen(input);
	uint64_t olen = len * 4 + 1;
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
	*op = 0;
	return output;
}

static int dir_exists(char *dirname)
{
	DIR *dir = opendir(dirname);
	if(dir == 0) return errno == EACCES;
	closedir(dir);
	return 1;
}

static Unit *build_unit(char *filename)
{
	char *unit_id = idfy(filename);
	char *cache_dir = 0;
	str_append(cache_dir, getenv("HOME"));
	str_append(cache_dir, "/.ja");
	
	if(!dir_exists(cache_dir)) {
		mkdir(cache_dir, 0755);
	}
	
	char *c_filename = 0;
	str_append(c_filename, cache_dir);
	str_append(c_filename, "/");
	str_append(c_filename, unit_id);
	str_append(c_filename, ".c");
	
	Unit *unit = malloc(sizeof(Unit));
	unit->src_filename = filename;
	unit->c_filename = c_filename;
	
	FILE *fs = fopen(filename, "rb");
	if(!fs) error("can not open input file '%s'", filename);
	fseek(fs, 0, SEEK_END);
	unit->src_len = ftell(fs);
	rewind(fs);
	unit->src = malloc(unit->src_len + 1);
	unit->src[unit->src_len] = 0;
	fread(unit->src, 1, unit->src_len, fs);
	fclose(fs);
	
	unit->tokens = lex(unit->src, unit->src_len);
	#ifdef JA_DEBUG
	print_tokens(unit->tokens);
	#endif
	
	unit->stmts = parse(unit->tokens);
	#ifdef JA_DEBUG
	print_ast(unit->stmts);
	#endif
	
	gen(unit);
	#ifdef JA_DEBUG
	print_c_code(unit->c_filename);
	#endif
	
	char *exe_filename = 0;
	str_append(exe_filename, cache_dir);
	str_append(exe_filename, "/");
	str_append(exe_filename, unit_id);
	
	char *cc_cmd = 0;
	str_append(cc_cmd, "gcc -o ");
	str_append(cc_cmd, exe_filename);
	str_append(cc_cmd, " ");
	str_append(cc_cmd, c_filename);
	
	int res = system(cc_cmd);
	if(res) error("could not compile the c code");
	
	system(exe_filename);
}

void build(char *main_filename)
{
	build_unit(realpath(main_filename, NULL));
}
