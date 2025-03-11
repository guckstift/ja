#ifndef parse_expr_H
#define parse_expr_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define parse_expr_C
#endif

#include "ast.c"

Expr *parse_expr();

#endif
#ifdef parse_expr_C

#define IMPLEMENT_FLAG

#include <stdlib.h>
#include "parse_impl.c"
#include "error.c"
#include "array.c"

static Expr *p_prefix();

static Expr **p_exprlist()
{
	Expr *first = 0;
	Expr *last = 0;
	int64_t count = 0;

	while(1) {
		Expr *expr = parse_expr();
		if(!expr) break;
		if(first) last = last->next = expr;
		else first = last = expr;
		count ++;
		if(!eat(PT_COMMA)) break;
	}

	Expr **exprs = 0;
	for(Expr *e = first; e; e = e->next) array_push(exprs, e);
	return exprs;
}

static Expr *p_atom()
{
	Token *start = peek();

	switch(advance()->kind) {
		case TK_INT:
			return create_int_expr(start->ival, .start = start);
		case KW_true: case KW_false:
			return create_bool_expr(start->kind == KW_true, .start = start);
		case KW_null:
			return create_null_expr(.start = start);
		case TK_IDENT:
			return create_var_expr(start->uid, .start = start);
		case TK_STRING:
			return create_string_expr(start->sval, .start = start);
	}

	setcur(start);
	return 0;
}

static Expr *p_postfix()
{
	Expr *expr = p_atom();

	if(!expr)
		return 0;

	while(1) {
		Expr *advanced_expr = 0;

		if(eat(PT_LBRACK)) {
			Expr *index = expect(K_EXPR, "expected index expression after [");
			expect(PT_RBRACK, "expected ] after index expression");
			advanced_expr = create_subscript_expr(expr, index, .start = expr->start);
		}
		else if(eat(PT_LPAREN)) {
			Expr **args = p_exprlist();
			expect(PT_RPAREN, "expected ) after (");
			advanced_expr = create_call_expr(expr, args, .start = expr->start);
		}

		if(!advanced_expr)
			break;

		expr = advanced_expr;
	}

	return expr;
}

static Expr *p_prefix()
{
	Token *start = peek();

	if(eat(PT_AMPERSAND)) {
		Expr *subexpr = p_prefix();

		if(!subexpr)
			error_at_cur("expected expression to point to");
		else if(!subexpr->islvalue)
			error_at(subexpr, "target is not addressable");

		return create_ptr_expr(subexpr, .start = start);
	}
	else if(eat(PT_STAR)) {
		Expr *subexpr = p_prefix();

		if(!subexpr)
			error_at_cur("expected expression to dereference");

		return create_deref_expr(subexpr, .start = start);
	}

	return p_postfix();
}

static Token *p_operator(OpLevel level)
{
	Token *op = 0;

	switch(level) {
		case OL_ADD:
			(op = eat(PT_PLUS)) ||
			(op = eat(PT_MINUS)) ;
			break;
		case OL_MUL:
			(op = eat(PT_MUL)) ;
			break;
	}

	return op;
}

static Expr *p_binop(int level)
{
	if(level == OPLEVEL_COUNT) return p_prefix();
	Expr *left = p_binop(level + 1);
	if(!left) return 0;

	while(1) {
		Token *operator = p_operator(level);
		if(!operator) break;

		Expr *right = p_binop(level + 1);
		if(!right) fatal_after(last, "expected right side after %t", operator);

		Expr *binop = new_binop_expr(left, right, operator, level);
		left = binop;
	}

	return left;
}

Expr *parse_expr()
{
	return p_prefix();
}

#endif