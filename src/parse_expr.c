#include <stdlib.h>
#include <stdio.h>
#include "parse_internal.h"
#include "array.h"

static Expr *p_expr();
static Expr **p_exprs();
static Expr *p_prefix();

static Type *p_type()
{
	ParseState state;
	pack_state(&state);
	Type *type = p_type_pub(&state);
	unpack_state(&state);
	return type;
}

static Expr *p_var()
{
	if(!eat(TK_IDENT)) return 0;
	Token *ident = last;
	return new_var_expr(ident, 0);
}

static Expr *p_new()
{
	if(!eat(TK_new)) return 0;
	Token *start = last;
	
	Token *t_start = cur;
	Type *obj_type = p_type();
	if(!obj_type) fatal_at(t_start, "expected type of object to create");
	
	return new_new_expr(start, obj_type);
}

static Expr *p_array()
{
	if(!eat(TK_LBRACK)) return 0;
	
	Token *start = last;
	Expr **items = 0;
	uint64_t length = 0;
	int isconst = 1;
	
	while(1) {
		Expr *item = p_expr();
		if(!item) break;
		
		isconst = isconst && item->isconst;
		array_push(items, item);
		length ++;
		
		if(!eat(TK_COMMA)) break;
	}
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected comma or ]");
	
	if(length == 0)
		fatal_at(last, "array literals can not be empty");
	
	return new_array_expr(start, items, isconst);
}

static Expr *p_atom()
{
	if(eat(TK_INT))
		return new_int_expr(last, last->ival);
	
	if(eat(TK_false) || eat(TK_true))
		return new_bool_expr(last, last->kind == TK_true);
	
	if(eat(TK_STRING))
		return new_string_expr(last, last->string, last->string_length);
		
	if(eat(TK_LPAREN)) {
		Token *start = last;
		Expr *expr = p_expr();
		expr->start = start;
		if(!eat(TK_RPAREN)) fatal_after(last, "expected )");
		return expr;
	}
	
	Expr *expr = 0;
	(expr = p_var()) ||
	(expr = p_new()) ||
	(expr = p_array()) ;
	return expr;
}

static Expr *p_call_x(Expr *expr)
{
	if(!eat(TK_LPAREN)) return 0;
	
	Type *type = expr->type;
	Expr **args = p_exprs();
	
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after argument list");
	
	return new_call_expr(expr, args);
}

static Expr *p_subscript_x(Expr *expr)
{
	if(!eat(TK_LBRACK)) return 0;
	
	Expr *index = p_expr();
	if(!index)
		fatal_after(last, "expected index expression after [");
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected ] after index expression");
	
	return new_subscript_expr(expr, index);
}

static Expr *p_member_x(Expr *object)
{
	if(!eat(TK_PERIOD)) return 0;
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
	Expr *expr = p_atom();
	if(!expr) return 0;
	
	while(1) {
		Expr *new_expr = 0;
		(new_expr = p_call_x(expr)) ||
		(new_expr = p_subscript_x(expr)) ||
		(new_expr = p_member_x(expr)) ;
		if(!new_expr) break;
		expr = new_expr;
	}
	
	return expr;
}

static Expr *p_ptr()
{
	if(!eat(TK_GREATER)) return 0;
	Token *start = last;
	
	Expr *subexpr = p_prefix();
	
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
	if(!eat(TK_LOWER)) return 0;
	Token *start = last;
	
	Expr *ptr = p_prefix();
	
	if(!ptr)
		fatal_at(last, "expected expression to dereference");
	
	if(ptr->kind == PTR)
		return ptr->subexpr;
	
	return new_deref_expr(start, ptr);
}

static Expr *p_negation()
{
	if(!eat(TK_MINUS)) return 0;
	Token *start = last;
	
	Expr *subexpr = p_prefix();
	
	if(!subexpr)
		fatal_at(last, "expected expression to negate");
	
	Expr *expr = new_expr(NEGATION, start, new_type(INT), subexpr->isconst, 0);
	expr->subexpr = subexpr;
	return expr;
}

static Expr *p_complement()
{
	if(!eat(TK_TILDE)) return 0;
	Token *start = last;
	
	Expr *subexpr = p_prefix();
	
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
	(expr = p_ptr()) ||
	(expr = p_deref()) ||
	(expr = p_negation()) ||
	(expr = p_complement()) ||
	(expr = p_postfix()) ;
	return expr;
}

static Expr *p_cast()
{
	Expr *expr = p_prefix();
	if(!eat(TK_as)) return expr;
	
	Type *type = p_type();
	if(!type)
		fatal_after(last, "expected type after as");
	
	return new_cast_expr(expr, type);
}

static Token *p_operator(OpLevel level)
{
	Token *op = 0;
	switch(level) {
		case OL_OR:
			(op = eat(TK_OR)) ;
			break;
		case OL_AND:
			(op = eat(TK_AND)) ;
			break;
		case OL_CMP:
			(op = eat(TK_LOWER)) ||
			(op = eat(TK_GREATER)) ||
			(op = eat(TK_EQUALS)) ||
			(op = eat(TK_NEQUALS)) ||
			(op = eat(TK_LEQUALS)) ||
			(op = eat(TK_GEQUALS)) ;
			break;
		case OL_ADD:
			(op = eat(TK_PLUS)) ||
			(op = eat(TK_MINUS)) ||
			(op = eat(TK_PIPE)) ||
			(op = eat(TK_XOR)) ;
			break;
		case OL_MUL:
			(op = eat(TK_MUL)) ||
			(op = eat(TK_DSLASH)) ||
			(op = eat(TK_MOD)) ||
			(op = eat(TK_AMP)) ;
			break;
	}
	return op;
}

static Expr *p_binop(int level)
{
	if(level == _OPLEVEL_COUNT) return p_cast();
	
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

static Expr *p_expr()
{
	return p_binop(0);
}

static Expr **p_exprs()
{
	Expr **exprs = 0;
	
	while(1) {
		Expr *expr = p_expr();
		if(!expr) break;
		array_push(exprs, expr);
		if(!eat(TK_COMMA)) break;
	}
	
	return exprs;
}

Expr *p_expr_pub(ParseState *state)
{
	unpack_state(state);
	Expr *expr = p_expr();
	pack_state(state);
	return expr;
}
