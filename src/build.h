#ifndef BUILD_H
#define BUILD_H

#include "lex.h"
#include "parse.h"

typedef struct Unit {
	int ismain;
	char *src_filename;
	char *unit_id;
	char *h_filename;
	char *c_filename;
	char *c_main_filename;
	char *src;
	int64_t src_len;
	Token *tokens;
	Block *block;
	char *obj_filename;
	char *obj_main_filename;
} Unit;

typedef struct {
	Unit **units;
	char *exe_filename;
} Project;

Project *build(char *main_filename, char *outfilename);
Unit *import(char *filename);

#endif
