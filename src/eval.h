#ifndef EVAL_H
#define EVAL_H

#include "parse.h"

Expr *eval_unary(Expr *expr);
Expr *eval_binop(Expr *expr);
Expr *eval_integral_cast(Expr *expr, Type *dtype);
Expr *eval_subscript(Expr *expr, Expr *index);

#endif
