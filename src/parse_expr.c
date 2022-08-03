#include <stdlib.h>
#include <stdio.h>
#include "parse_internal.h"
#include "eval.h"
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

/*
	Might modify expr and type
*/
Expr *cast_expr(Expr *expr, Type *type, int explicit)
{
	Type *stype = expr->type;
	
	// can not cast from none type
	if(stype->kind == NONE)
		fatal_at(expr->start, "expression has no value");
	
	// types equal => no cast needed
	if(type_equ(stype, type))
		return expr;
	
	// integral types are castable into other integral types
	if(is_integral_type(stype) && is_integral_type(type))
		return eval_integral_cast(expr, type);
	
	// one pointer to some other by explicit cast always ok
	if(explicit && stype->kind == PTR && type->kind == PTR)
		return new_cast_expr(expr, type);
	
	// string to cstring is also okay
	if(stype->kind == STRING && type->kind == CSTRING)
		return new_cast_expr(expr, type);
	
	// array ptr to dynamic array ptr when same item type
	if(
		stype->kind == PTR && stype->subtype->kind == ARRAY &&
		is_dynarray_ptr_type(type) &&
		type_equ(stype->subtype->itemtype, type->subtype->itemtype)
	) {
		return new_cast_expr(expr, type);
	}
	
	// arrays with equal length
	if(
		stype->kind == ARRAY && type->kind == ARRAY &&
		stype->length == type->length
	) {
		// array literal => cast each item to itemtype
		if(expr->kind == ARRAY) {
			array_for(expr->items, i) {
				expr->items[i] = cast_expr(
					expr->items[i], type->itemtype, explicit
				);
			}
			stype->itemtype = type->itemtype;
			return expr;
		}
		// no array literal => create new array literal with cast items
		else {
			Expr **new_items = 0;
			
			for(int64_t i=0; i < stype->length; i++) {
				Expr *index = new_int_expr(expr->start, i);
				Expr *subscript = new_subscript_expr(expr, index);
				Expr *item = cast_expr(subscript, type->itemtype, explicit);
				array_push(new_items, item);
			}
			
			return new_array_expr(
				new_items[0]->start, new_items, expr->isconst
			);
		}
	}
	
	fatal_at(
		expr->start,
		"can not convert type  %y  to  %y  (%s)",
		stype, type, explicit ? "explicit" : "implicit"
	);
}

static Expr *p_var()
{
	if(!eat(TK_IDENT)) return 0;
	Token *ident = last;
	//Decl *decl = lookup(ident->id);
	
	/*
	if(decl->kind == STRUCT)
		fatal_at(last, "%t is the name of a structure", ident);
	*/
	
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

static Expr *p_cast_x(Expr *expr)
{
	if(!eat(TK_as)) return 0;
	
	Type *type = p_type();
	if(!type)
		fatal_after(last, "expected type after as");
	
	return cast_expr(expr, type, 1);
}

static Expr *p_call_x(Expr *expr)
{
	if(!eat(TK_LPAREN)) return 0;
	
	/*
	if(expr->type->kind != FUNC)
		fatal_at(expr->start, "not a function you are calling");
	*/
	
	Type *type = expr->type;
	//Type **paramtypes = type->paramtypes;
	Expr **args = p_exprs();
	
	/*
	if(array_length(args) < array_length(paramtypes)) {
		fatal_at(
			last, "not enough arguments, %i needed", array_length(paramtypes)
		);
	}
	else if(array_length(args) > array_length(paramtypes)) {
		fatal_at(
			last, "too many arguments, %i needed", array_length(paramtypes)
		);
	}
	*/
	
	/*
	array_for(args, i) {
		args[i] = cast_expr(args[i], paramtypes[i], 0);
	}
	*/
	
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after argument list");
	
	return new_call_expr(expr, args);
}

static Expr *p_subscript_x(Expr *expr)
{
	if(!eat(TK_LBRACK)) return 0;
	
	/*
	while(expr->type->kind == PTR) {
		expr = new_deref_expr(expr->start, expr);
	}
	*/
	
	/*
	if(expr->type->kind != ARRAY && expr->type->kind != STRING) {
		fatal_after(last, "need array or string to subscript");
	}
	*/
	
	Expr *index = p_expr();
	if(!index)
		fatal_after(last, "expected index expression after [");
	
	/*
	if(!is_integral_type(index->type))
		fatal_at(index->start, "index is not integral");
	*/
	
	/*
	if(
		expr->type->kind == ARRAY &&
		expr->type->length >= 0 &&
		index->isconst
	) {
		if(index->value < 0 || index->value >= expr->type->length)
			fatal_at(
				index->start,
				"index is out of range, must be between 0 .. %u",
				expr->type->length - 1
			);
	}
	*/
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected ] after index expression");
	
	return new_subscript_expr(expr, index);
	//return eval_subscript(expr, index);
}

static Expr *p_member_x(Expr *expr)
{
	if(!eat(TK_PERIOD)) return 0;
	
	/*
	while(expr->type->kind == PTR) {
		expr = new_deref_expr(expr->start, expr);
	}
	*/
	
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_at(last, "expected id of member to access");
	
	Type *type = expr->type;
	
	/*
	if(
		(type->kind == ARRAY || type->kind == STRING) &&
		tokequ_str(ident, "length")
	) {
		return new_length_expr(expr);
	}
	*/
	
	/*
	if(type->kind != STRUCT && type->kind != ENUM && type->kind != UNION)
		fatal_at(expr->start, "no instance or enum to get member from");
	*/
	
	// TODO: postpone type checking
	
	if(type->kind == STRUCT || type->kind == UNION) {
		Decl **members = type->decl->members;
		Decl *member = 0;
		
		array_for(members, i) {
			if(members[i]->id == ident->id) {
				member = members[i];
				break;
			}
		}
		
		if(!member) fatal_at(ident, "name %t not declared in struct", ident);
		return new_member_expr(expr, member);
	}
	else if(type->kind == ENUM) {
		Decl *enumdecl = type->decl;
		EnumItem **items = enumdecl->items;
		EnumItem *item = 0;
		
		array_for(items, i) {
			if(items[i]->id == ident->id) {
				item = items[i];
				break;
			}
		}
		
		if(!item) fatal_at(ident, "name %t not declared in enum", ident);
		return new_enum_item_expr(expr->start, enumdecl, item);
	}
	
	return 0;
}

static Expr *p_postfix()
{
	Expr *expr = p_atom();
	if(!expr) return 0;
	
	while(1) {
		Expr *new_expr = 0;
		(new_expr = p_cast_x(expr)) ||
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
	
	/*
	if(!is_integral_type(subexpr->type))
		fatal_at(subexpr->start, "expected integral type to negate");
	*/
	
	if(subexpr->kind == NEGATION)
		return subexpr->subexpr;
	
	Expr *expr = new_expr(NEGATION, start, new_type(INT), subexpr->isconst, 0);
	expr->subexpr = subexpr;
	return eval_unary(expr);
}

static Expr *p_complement()
{
	if(!eat(TK_TILDE)) return 0;
	Token *start = last;
	
	Expr *subexpr = p_prefix();
	
	if(!subexpr)
		fatal_at(last, "expected expression to complement");
	
	/*
	if(!is_integral_type(subexpr->type))
		fatal_at(subexpr->start, "expected integral type to complement");
	*/
	
	if(subexpr->kind == COMPLEMENT)
		return subexpr->subexpr;
	
	Expr *expr = new_expr(
		COMPLEMENT, start, new_type(INT), subexpr->isconst, 0
	);
	
	expr->subexpr = subexpr;
	return eval_unary(expr);
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

typedef enum {
	OL_OR,
	OL_AND,
	OL_CMP,
	OL_ADD,
	OL_MUL,
	
	_OPLEVEL_COUNT,
} OpLevel;

static Token *p_operator(int level)
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
	if(level == _OPLEVEL_COUNT) return p_prefix();
	
	Expr *left = p_binop(level + 1);
	if(!left) return 0;
	
	while(1) {
		Token *operator = p_operator(level);
		if(!operator) break;
		
		Expr *right = p_binop(level + 1);
		if(!right) fatal_after(last, "expected right side after %t", operator);
		
		/*
		Type *ltype = left->type;
		Type *rtype = right->type;
		Type *type = 0;
		*/
		
		int found = 0;
		
		if(level == OL_OR || level == OL_AND) {
			/*
			if(!type_equ(ltype, rtype)) {
				fatal_at(operator,
					"types must be the same for operator %t", operator
				);
			}
			type = ltype;
			*/
			
			found = 1;
		}
		/*
		else if(is_integral_type(ltype) && is_integral_type(rtype)) {
			if(level == OL_CMP) {
				//type = new_type(BOOL);
				//right = cast_expr(right, ltype, 0);
				found = 1;
			}
			else if(level == OL_ADD || level == OL_MUL) {
				//type = new_type(INT64);
				//left = cast_expr(left, type, 0);
				//right = cast_expr(right, type, 0);
				found = 1;
			}
		}
		* /
		else if(
			ltype->kind == STRING && rtype->kind == STRING &&
			operator->kind == TK_EQUALS
		) {
			type = new_type(BOOL);
			found = 1;
		}
		
		if(found == 0) {
			fatal_at(
				operator,
				"can not use types  %y  and  %y  with operator %t",
				ltype, rtype, operator
			);
		}
		*/
		
		//Expr *binop = new_binop_expr(left, right, operator, type);
		Expr *binop = new_binop_expr(left, right, operator, 0);
		binop = eval_binop(binop);
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
