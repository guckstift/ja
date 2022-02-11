#ifndef PRINT_H
#define PRINT_H

#include <stdarg.h>
#include "lex.h"
#include "parse.h"

void vprint_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg,
	va_list args
);

void print_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg, ...
);

void print_tokens(Tokens *tokens);
void print_ast(Stmt *stmts);
void print_c_code(char *c_filename);
void print_tree(Node *tree);

#endif
