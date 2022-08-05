#include <stdio.h>
#include "eval.h"

Expr *eval_unary(Expr *expr)
{
	if(!expr->isconst) return expr;
	
	switch(expr->kind) {
		case NEGATION:
			expr->value = -expr->subexpr->value;
			expr->kind = INT;
			break;
		case COMPLEMENT:
			expr->value = ~expr->subexpr->value;
			expr->kind = INT;
			break;
	}
	
	return expr;
}
