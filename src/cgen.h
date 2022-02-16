#ifndef CGEN_H
#define CGEN_H

#include "parse.h"
#include "build.h"

void write(char *msg, ...);
void gen_init_expr(Expr *expr);
void gen_expr(Expr *expr);
void gen(Unit *unit);

#endif
