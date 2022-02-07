#include <stdio.h>
#include "eval.h"

Expr *eval_binop(Expr *expr)
{
	if(expr->isconst) {
		switch(expr->operator->type) {
			case TK_PLUS: {
				if(is_integral_type(expr->dtype)) {
					expr->type = EX_INT;
					expr->ival = expr->left->ival + expr->right->ival;
				}
				break;
			}
			case TK_MINUS: {
				if(is_integral_type(expr->dtype)) {
					expr->type = EX_INT;
					expr->ival = expr->left->ival - expr->right->ival;
				}
				break;
			}
		}
	}
	
	return expr;
}

Expr *eval_integral_cast(Expr *expr, TypeDesc *dtype)
{
	TypeDesc *src_type = expr->dtype;
	
	if(expr->isconst) {
		expr->dtype = dtype;
		
		switch(dtype->type) {
			case TY_UINT8:
				expr->type = EX_INT;
				expr->ival = (uint8_t)expr->ival;
				break;
			case TY_INT64:
			case TY_UINT64:
				expr->type = EX_INT;
				break;
			case TY_BOOL:
				expr->type = EX_BOOL;
				expr->ival = expr->ival != 0;
				break;
		}
		
		return expr;
	}
	
	return new_cast_expr(expr, dtype);
}

Expr *eval_subscript(Expr *expr, Expr *index)
{
	if(expr->type == EX_ARRAY && index->isconst) {
		expr = expr->exprs;
		while(index->ival) {
			expr = expr->next;
			index->ival --;
		}
		return expr;
	}
	
	return new_subscript(expr, index);
}
