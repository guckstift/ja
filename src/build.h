#ifndef BUILD_H
#define BUILD_H

#include "lex.h"
#include "parse.h"

typedef struct Unit {
	int ismain;
	struct Unit *next;
	char *src_filename;
	char *unit_id;
	char *h_filename;
	char *c_filename;
	char *c_main_filename;
	char *src;
	int64_t src_len;
	Tokens *tokens;
	struct Stmt *stmts;
	char *obj_filename;
	char *obj_main_filename;
} Unit;

typedef struct {
	Unit *first;
	Unit *last;
} Project;

void build(char *main_filename);
Unit *import(char *filename);

#endif
