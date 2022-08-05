#ifndef EVAL_H
#define EVAL_H

#include "parse.h"

Expr *eval_unary(Expr *expr);
Expr *eval_binop(Expr *expr);

#endif
