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
	struct Stmt **stmts;
	char *obj_filename;
	char *obj_main_filename;
} Unit;

typedef struct {
	Unit **units;
} Project;

void build(char *main_filename);
Unit *import(char *filename);

#endif
