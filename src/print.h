#ifndef PRINT_H
#define PRINT_H

#include <stdarg.h>
#include <stdio.h>
#include "lex.h"
#include "parse.h"

#define COL_RESET   "\x1b[0m"
#define COL_GREY    "\x1b[38;2;170;170;170m"
#define COL_BLUE    "\x1b[38;2;64;128;255m"
#define COL_MAGENTA "\x1b[38;2;255;64;255m"
#define COL_AQUA    "\x1b[38;2;64;255;255m"
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RED     "\x1b[1;31m"
#define COL_YELLOW_BG "\x1b[43m"

void ja_vfprintf(FILE *fs, char *msg, va_list args);
void ja_fprintf(FILE *fs, char *msg, ...);
void ja_printf(char *msg, ...);

void vprint_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg,
	va_list args
);

void print_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg, ...
);

void print_tokens(Token *tokens);
void print_ast(Block *block);
void print_c_code(char *c_filename);

#endif
