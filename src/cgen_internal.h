#ifndef CGEN_INTERNAL_H
#define CGEN_INTERNAL_H

#include "cgen.h"

#define INDENT      "    "
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

void write(char *msg, ...);
int is_in_header();
void inc_level();
void dec_level();
Unit *get_cur_unit();
void gen_type_postfix(Type *dtype);
void gen_type(Type *dtype);
void gen_init_expr(Expr *expr);
void gen_expr(Expr *expr);
void gen_stmt(Stmt *stmt, int noindent);
void gen_stmts(Stmt **stmts);
void gen_block(Block *block);
void gen_vardecl(Decl *decl);
void gen_mainfuncname(Unit *unit);

#endif
