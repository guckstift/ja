#ifndef PRINT_H
#define PRINT_H

#include <stdio.h>
#include <stdarg.h>
#include "ast.h"

#define COL_RESET   "\x1b[0m"
#define COL_BOLD    "\x1b[1m"
#define COL_RED     "\x1b[38;2;255;0;0m"
#define COL_MAGENTA "\x1b[38;2;255;64;255m"
#define COL_GREEN   "\x1b[38;2;0;255;0m"
#define COL_BLUE    "\x1b[38;2;64;128;255m"
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_DGREY   "\x1b[38;2;85;85;85m"
#define COL_GREY    "\x1b[38;2;128;128;128m"

#define BGCOL_XDGREY  "\x1b[48;2;32;32;32m"

#define COL_KW(x)   COL_BLUE    x COL_RESET
#define COL_LIT(x)  COL_MAGENTA x COL_RESET
#define COL_OK(x)   COL_GREEN   x COL_RESET
#define COL_FAIL(x) COL_RED     x COL_RESET

#define print(...)              fprint(stdout, __VA_ARGS__)
#define print_tokens(...)       fprint_tokens(stdout, __VA_ARGS__)
#define print_tokens_short(...) fprint_tokens_short(stdout, __VA_ARGS__)
#define print_type(...)         fprint_type(stdout, __VA_ARGS__)
#define print_expr(...)         fprint_expr(stdout, __VA_ARGS__)
#define print_stmt(...)         fprint_stmt(stdout, __VA_ARGS__)
#define print_block(...)        fprint_block(stdout, __VA_ARGS__)
#define print_ast(...)          fprint_ast(stdout, __VA_ARGS__)

typedef char *(*EscapeMod)(FILE*, char*, va_list);

typedef struct {
	int argc;
	char *argv[4];
} ArgLog;

ArgLog get_arglog();
void inclevel();
void declevel();
void reset_escapemods();
void register_escapemod(char chr, EscapeMod func);
void vfprint(FILE *fs, char *msg, va_list args);
void fprint(FILE *fs, char *msg, ...);
void fprint_tokens(FILE *fs, Token *tokens);
void fprint_tokens_short(FILE *fs, Token *tokens);
void fprint_type(FILE *fs, Type *type);
void fprint_expr(FILE *fs, Expr *expr);
void fprint_stmt(FILE *fs, Stmt *stmt);
void fprint_block(FILE *fs, Block *block);
void fprint_ast(FILE *fs, Block *block);
void print_c_code(char *cfile);

#endif