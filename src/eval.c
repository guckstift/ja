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

Expr *eval_binop(Expr *expr)
{
	Expr *left = expr->left;
	Expr *right = expr->right;
	
	if(expr->isconst) {
		switch(expr->operator->kind) {
			#define INT_BINOP(name, op) \
				case TK_ ## name: { \
					if(is_integral_type(expr->type)) { \
						expr->kind = INT; \
						expr->value = expr->left->value op \
							expr->right->value; \
					} \
					break; \
				}
			
			#define CMP_BINOP(name, op) \
				case TK_ ## name: { \
					if(is_integral_type(expr->left->type)) { \
						expr->kind = BOOL; \
						expr->value = expr->left->value op \
							expr->right->value; \
					} \
					break; \
				}
			
			INT_BINOP(PLUS, +)
			INT_BINOP(MINUS, -)
			INT_BINOP(MUL, *)
			INT_BINOP(DSLASH, /)
			INT_BINOP(MOD, %)
			INT_BINOP(AMP, &)
			INT_BINOP(PIPE, |)
			INT_BINOP(XOR, ^)
			
			CMP_BINOP(LOWER, <)
			CMP_BINOP(GREATER, >)
			CMP_BINOP(EQUALS, ==)
			CMP_BINOP(NEQUALS, !=)
			CMP_BINOP(LEQUALS, <=)
			CMP_BINOP(GEQUALS, >=)
			
			case TK_AND:
				if(expr->type->kind == STRING) {
					expr->kind = STRING;
					if(left->length == 0) {
						expr->string = left->string;
						expr->length = left->length;
					}
					else {
						expr->string = right->string;
						expr->length = right->length;
					}
				}
				else if(is_integral_type(expr->type)) {
					expr->kind = left->kind;
					if(left->value == 0) {
						expr->value = left->value;
					}
					else {
						expr->value = right->value;
					}
				}
				break;
			
			case TK_OR:
				if(expr->type->kind == STRING) {
					expr->kind = STRING;
					if(left->length != 0) {
						expr->string = left->string;
						expr->length = left->length;
					}
					else {
						expr->string = right->string;
						expr->length = right->length;
					}
				}
				else if(is_integral_type(expr->type)) {
					expr->kind = left->kind;
					if(left->value != 0) {
						expr->value = left->value;
					}
					else {
						expr->value = right->value;
					}
				}
				break;
		}
	}
	
	return expr;
}

Expr *eval_integral_cast(Expr *expr, Type *type)
{
	Type *src_type = expr->type;
	
	if(expr->isconst) {
		expr->type = type;
		expr->kind = INT;
		
		switch(type->kind) {
			case INT8:
				expr->value = (int8_t)expr->value;
				break;
			case UINT8:
				expr->value = (uint8_t)expr->value;
				break;
			case INT16:
				expr->value = (int16_t)expr->value;
				break;
			case UINT16:
				expr->value = (uint16_t)expr->value;
				break;
			case INT32:
				expr->value = (int32_t)expr->value;
				break;
			case UINT32:
				expr->value = (uint32_t)expr->value;
				break;
			case INT64:
			case UINT64:
				break;
			case BOOL:
				expr->kind = BOOL;
				expr->value = expr->value != 0;
				break;
		}
		
		return expr;
	}
	
	return new_cast_expr(expr, type);
}

Expr *eval_subscript(Expr *expr, Expr *index)
{
	if(expr->kind == ARRAY && index->isconst) {
		return expr->items[index->value];
	}
	
	return new_subscript_expr(expr, index);
}
