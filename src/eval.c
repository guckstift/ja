#include <stdio.h>
#include "eval.h"

Expr *eval_expr(Expr *expr)
{
	if(!expr) return 0;
	
	switch(expr->type) {
		case EX_PTR: {
			expr->subexpr = eval_expr(expr->subexpr);
			if(expr->subexpr->type == EX_DEREF) {
				expr = expr->subexpr->subexpr;
			}
			break;
		}
		case EX_DEREF: {
			expr->subexpr = eval_expr(expr->subexpr);
			if(expr->subexpr->type == EX_PTR) {
				expr = expr->subexpr->subexpr;
			}
			break;
		}
		case EX_CAST: {
			Expr *subexpr = expr->subexpr = eval_expr(expr->subexpr);
			TypeDesc *src_type = subexpr->dtype;
			TypeDesc *dtype = expr->dtype;
			if(subexpr->isconst) {
				if(is_integral_type(src_type)) {
					if(dtype->type == TY_BOOL) {
						expr->type = EX_BOOL;
						expr->ival = subexpr->ival ? 1 : 0;
					}
					else if(is_integer_type(dtype)) {
						expr->type = EX_INT;
						expr->ival = subexpr->ival;
					}
				}
			}
			
			// int and bool literals can be converted on the fly
			if(
				(subexpr->type == EX_INT || subexpr->type == EX_BOOL) &&
				is_integer_type(dtype)
			) {
				switch(dtype->type) {
					case TY_UINT8:
						subexpr->ival = (uint8_t)subexpr->ival;
						break;
				}
				subexpr->type = EX_INT;
				subexpr->dtype = dtype;
				return subexpr;
			}
			
			break;
		}
		case EX_SUBSCRIPT: {
			expr->subexpr = eval_expr(expr->subexpr);
			expr->index = eval_expr(expr->index);
			if(expr->subexpr->type == EX_ARRAY && expr->index->isconst) {
				Expr *array = expr->subexpr;
				Expr *index = expr->index;
				int64_t index_val = index->ival;
				Expr *item = array->exprs;
				while(index_val) {
					item = item->next;
					index_val --;
				}
				item->next = expr->next;
				expr = item;
			}
			break;
		}
		case EX_BINOP: {
			expr->left = eval_expr(expr->left);
			expr->right = eval_expr(expr->right);
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
			break;
		}
		case EX_ARRAY: {
			Expr *prev = 0;
			for(Expr *item = expr->exprs; item; item = item->next) {
				Expr *new_item = eval_expr(item);
				if(prev) {
					prev->next = new_item;
				}
				else {
					expr->exprs = new_item;
				}
				new_item->next = item->next;
				item = new_item;
				prev = item;
			}
			break;
		}
	}
	
	return expr;
}
