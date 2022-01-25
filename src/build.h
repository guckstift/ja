#ifndef BUILD_H
#define BUILD_H

#include "lex.h"
#include "parse.h"

typedef struct {
	char *src_filename;
	char *c_filename;
	char *src;
	int64_t src_len;
	Tokens *tokens;
	Stmt *stmts;
} Unit;

void build(char *main_filename);

#endif
