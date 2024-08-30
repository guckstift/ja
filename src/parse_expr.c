#include <stdlib.h>
#include "parse_expr.h"
#include "parse_impl.h"
#include "error.h"

static Expr *p_prefix();

static Expr *p_atom()
{
	Token *start = peek();

	switch(advance()->kind) {
		case TK_INT:
		case KW_true: case KW_false:
		case KW_null:
		case TK_IDENT:
			return expr_from_token(getlast());
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

Expr *parse_expr()
{
	return p_prefix();
}