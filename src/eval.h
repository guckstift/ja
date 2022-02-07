#ifndef EVAL_H
#define EVAL_H

#include "parse.h"

Expr *eval_binop(Expr *expr);
Expr *eval_integral_cast(Expr *expr, TypeDesc *dtype);
Expr *eval_subscript(Expr *expr, Expr *index);

#endif
