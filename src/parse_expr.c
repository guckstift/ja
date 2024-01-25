#include <stdlib.h>
#include <stdio.h>
#include "parse_internal.h"
#include "array.h"

static Expr **p_exprs();
static Expr *p_prefix();

static Expr *p_var()
{
	if(!eat(TK_IDENT)) return 0;
	Token *ident = last;
	return new_var_expr(ident, 0);
}

static Expr *p_new()
{
	if(!eatkw(KW_new)) return 0;
	Token *start = last;

	Token *t_start = cur;
	Type *obj_type = p_(type);
	if(!obj_type) fatal_at(t_start, "expected type of object to create");

	return new_new_expr(start, obj_type);
}

static Expr *p_array()
{
	if(!eatpt(PT_LBRACK)) return 0;

	Token *start = last;
	Expr **items = 0;
	uint64_t length = 0;
	int isconst = 1;

	while(1) {
		Expr *item = p_(expr);
		if(!item) break;

		isconst = isconst && item->isconst;
		array_push(items, item);
		length ++;

		if(!eatpt(PT_COMMA)) break;
	}

	if(!eatpt(PT_RBRACK))
		fatal_after(last, "expected comma or ]");

	if(length == 0)
		fatal_at(last, "array literals can not be empty");

	return new_array_expr(start, items, isconst);
}

static Expr *p_atom()
{
	if(eat(TK_INT))
		return new_int_expr(last, last->ival);

	if(eatkw(KW_false) || eatkw(KW_true))
		return new_bool_expr(last, last->keyword == KW_true);

	if(eat(TK_STRING))
		return new_string_expr(last, last->string, last->string_length);

	if(eatpt(PT_LPAREN)) {
		Token *start = last;
		Expr *expr = p_(expr);
		expr->start = start;
		if(!eatpt(PT_RPAREN)) fatal_after(last, "expected )");
		return expr;
	}

	Expr *expr = 0;
	(expr = p_(var)) ||
	(expr = p_(new)) ||
	(expr = p_(array)) ;
	return expr;
}

static Expr *p_call_x(Expr *expr)
{
	if(!eatpt(PT_LPAREN)) return 0;

	Type *type = expr->type;
	Expr **args = p_(exprs);

	if(!eatpt(PT_RPAREN))
		fatal_after(last, "expected ) after argument list");

	return new_call_expr(expr, args);
}

static Expr *p_subscript_x(Expr *expr)
{
	if(!eatpt(PT_LBRACK)) return 0;

	Expr *index = p_(expr);
	if(!index)
		fatal_after(last, "expected index expression after [");

	if(!eatpt(PT_RBRACK))
		fatal_after(last, "expected ] after index expression");

	return new_subscript_expr(expr, index);
}

static Expr *p_member_x(Expr *object)
{
	if(!eatpt(PT_PERIOD)) return 0;
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_at(last, "expected id of member to access");

	if(object->kind == VAR) {
		Token *object_id = object->id;
		Decl *decl = lookup(object_id);

		if(decl && decl->kind == ENUM) {
			EnumItem **items = decl->items;
			EnumItem *item = 0;

			array_for(items, i) {
				if(items[i]->id == ident->id) {
					item = items[i];
					break;
				}
			}

			if(!item) fatal_at(ident, "name %t not declared in enum", ident);
			return new_enum_item_expr(object->start, decl, item);
		}
	}

	return new_member_expr(object, ident->id);
}

static Expr *p_postfix()
{
	Expr *expr = p_(atom);
	if(!expr) return 0;

	while(1) {
		Expr *new_expr = 0;
		(new_expr = p_(call_x, expr)) ||
		(new_expr = p_(subscript_x, expr)) ||
		(new_expr = p_(member_x, expr)) ;
		if(!new_expr) break;
		expr = new_expr;
	}

	return expr;
}

static Expr *p_ptr()
{
	if(!eatpt(PT_AMP)) return 0;
	Token *start = last;

	Expr *subexpr = p_(prefix);

	if(!subexpr)
		fatal_after(last, "expected expression to point to");

	if(!subexpr->islvalue)
		fatal_at(subexpr->start, "target is not addressable");

	if(subexpr->kind == DEREF)
		return subexpr->ptr;

	return new_ptr_expr(start, subexpr);
}

static Expr *p_deref()
{
	if(!eatpt(PT_LOWER)) return 0;
	Token *start = last;

	Expr *ptr = p_(prefix);

	if(!ptr)
		fatal_at(last, "expected expression to dereference");

	if(ptr->kind == PTR)
		return ptr->subexpr;

	return new_deref_expr(start, ptr);
}

static Expr *p_negation()
{
	if(!eatpt(PT_MINUS)) return 0;
	Token *start = last;

	Expr *subexpr = p_(prefix);

	if(!subexpr)
		fatal_at(last, "expected expression to negate");

	Expr *expr = new_expr(NEGATION, start, new_type(INT), subexpr->isconst, 0);
	expr->subexpr = subexpr;
	return expr;
}

static Expr *p_complement()
{
	if(!eatpt(PT_TILDE)) return 0;
	Token *start = last;

	Expr *subexpr = p_(prefix);

	if(!subexpr)
		fatal_at(last, "expected expression to complement");

	Expr *expr = new_expr(
		COMPLEMENT, start, new_type(INT), subexpr->isconst, 0
	);

	expr->subexpr = subexpr;
	return expr;
}

static Expr *p_prefix()
{
	Expr *expr = 0;
	(expr = p_(ptr)) ||
	(expr = p_(deref)) ||
	(expr = p_(negation)) ||
	(expr = p_(complement)) ||
	(expr = p_(postfix)) ;
	return expr;
}

static Expr *p_cast()
{
	Expr *expr = p_(prefix);
	if(!eatkw(KW_as)) return expr;

	Type *type = p_(type);
	if(!type)
		fatal_after(last, "expected type after as");

	return new_cast_expr(expr, type);
}

static Token *p_operator(OpLevel level)
{
	Token *op = 0;
	switch(level) {
		case OL_OR:
			(op = eatpt(PT_OR)) ;
			break;
		case OL_AND:
			(op = eatpt(PT_AND)) ;
			break;
		case OL_CMP:
			(op = eatpt(PT_LOWER)) ||
			(op = eatpt(PT_GREATER)) ||
			(op = eatpt(PT_EQUALS)) ||
			(op = eatpt(PT_NEQUALS)) ||
			(op = eatpt(PT_LEQUALS)) ||
			(op = eatpt(PT_GEQUALS)) ;
			break;
		case OL_ADD:
			(op = eatpt(PT_PLUS)) ||
			(op = eatpt(PT_MINUS)) ||
			(op = eatpt(PT_PIPE)) ||
			(op = eatpt(PT_XOR)) ;
			break;
		case OL_MUL:
			(op = eatpt(PT_MUL)) ||
			(op = eatpt(PT_DSLASH)) ||
			(op = eatpt(PT_MOD)) ||
			(op = eatpt(PT_AMP)) ;
			break;
	}
	return op;
}

static Expr *p_binop(int level)
{
	if(level == _OPLEVEL_COUNT) return p_(cast);

	Expr *left = p_(binop, level + 1);
	if(!left) return 0;

	while(1) {
		Token *operator = p_(operator, level);
		if(!operator) break;

		Expr *right = p_(binop, level + 1);
		if(!right) fatal_after(last, "expected right side after %t", operator);

		Expr *binop = new_binop_expr(left, right, operator, level);
		left = binop;
	}

	return left;
}

Expr *p_expr()
{
	return p_(binop, 0);
}

static Expr **p_exprs()
{
	Expr **exprs = 0;

	while(1) {
		Expr *expr = p_(expr);
		if(!expr) break;
		array_push(exprs, expr);
		if(!eatpt(PT_COMMA)) break;
	}

	return exprs;
}