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
