#include <stdio.h>
#include "eval.h"

Expr *eval_binop(Expr *expr)
{
	if(expr->isconst) {
		switch(expr->operator->type) {
			case TK_PLUS: {
				if(is_integral_type(expr->dtype)) {
					expr->kind = INT;
					expr->ival = expr->left->ival + expr->right->ival;
				}
				break;
			}
			case TK_MINUS: {
				if(is_integral_type(expr->dtype)) {
					expr->kind = INT;
					expr->ival = expr->left->ival - expr->right->ival;
				}
				break;
			}
		}
	}
	
	return expr;
}

Expr *eval_integral_cast(Expr *expr, Type *dtype)
{
	Type *src_type = expr->dtype;
	
	if(expr->isconst) {
		expr->dtype = dtype;
		expr->kind = INT;
		
		switch(dtype->kind) {
			case INT8:
				expr->ival = (int8_t)expr->ival;
				break;
			case UINT8:
				expr->ival = (uint8_t)expr->ival;
				break;
			case INT16:
				expr->ival = (int16_t)expr->ival;
				break;
			case UINT16:
				expr->ival = (uint16_t)expr->ival;
				break;
			case INT32:
				expr->ival = (int32_t)expr->ival;
				break;
			case UINT32:
				expr->ival = (uint32_t)expr->ival;
				break;
			case INT64:
			case UINT64:
				break;
			case BOOL:
				expr->kind = BOOL;
				expr->ival = expr->ival != 0;
				break;
		}
		
		return expr;
	}
	
	return new_cast_expr(expr, dtype);
}

Expr *eval_subscript(Expr *expr, Expr *index)
{
	if(expr->kind == ARRAY && index->isconst) {
		expr = expr->exprs;
		while(index->ival) {
			expr = expr->next;
			index->ival --;
		}
		return expr;
	}
	
	return new_subscript(expr, index);
}
