#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "parse.h"
#include "print.h"
#include "eval.h"
#include "build.h"
#include "utils.h"

static Stmt *p_stmts(Decl *func);
static Expr *p_expr();
static Type *p_type();
static Expr *p_prefix();

#include "parse_utils.h"

static int declare(Decl *new_decl)
{
	if(new_decl->scope != scope) {
		// from foreign scope => import
		new_decl = (Decl*)clone_stmt((Stmt*)new_decl);
		new_decl->next_decl = 0;
		new_decl->imported = 1;
	}
	else {
		new_decl->public_id = 0;
		str_append(new_decl->public_id, "_");
		str_append(new_decl->public_id, unit_id);
		str_append(new_decl->public_id, "_");
		str_append_token(new_decl->public_id, new_decl->id);
	}
	
	if(lookup_flat(new_decl->id)) {
		return 0;
	}
	
	list_push(scope, first_decl, last_decl, next_decl, new_decl);
	
	return 1;
}

static void enter()
{
	Scope *new_scope = malloc(sizeof(Scope));
	new_scope->parent = scope;
	scope = new_scope;
	scope->first_decl = 0;
	scope->last_decl = 0;
	scope->func = scope->parent ? scope->parent->func : 0;
	scope->struc = 0;
	scope->first_import = 0;
	scope->last_import = 0;
}

static void leave()
{
	scope = scope->parent;
}

static Type *p_primtype()
{
	if(eat(TK_int)) return new_type(INT);
	if(eat(TK_int8)) return new_type(INT8);
	if(eat(TK_int16)) return new_type(INT16);
	if(eat(TK_int32)) return new_type(INT32);
	if(eat(TK_int64)) return new_type(INT64);
	if(eat(TK_uint)) return new_type(UINT);
	if(eat(TK_uint8)) return new_type(UINT8);
	if(eat(TK_uint16)) return new_type(UINT16);
	if(eat(TK_uint32)) return new_type(UINT32);
	if(eat(TK_uint64)) return new_type(UINT64);
	if(eat(TK_bool)) return new_type(BOOL);
	if(eat(TK_string)) return new_type(STRING);
	return 0;
}

static Type *p_nametype()
{
	Token *ident = eat(TK_IDENT);
	if(!ident) return 0;
	
	Decl *decl = lookup(ident->id);
	
	if(!decl)
		fatal_at(ident, "name %t not declared", ident);
	
	if(decl->kind != STRUCT)
		fatal_at(ident, "%t is not a structure", ident);
	
	Type *dtype = new_type(STRUCT);
	dtype->id = ident->id;
	dtype->typedecl = decl;
	return dtype;
}

static Type *p_ptrtype()
{
	if(!eat(TK_GREATER)) return 0;
	
	Type *subtype = p_type();
	if(!subtype)
		fatal_at(last, "expected target type");
	
	return new_ptr_type(subtype);
}

static Type *p_arraytype()
{
	if(!eat(TK_LBRACK)) return 0;
	
	Token *length = eat(TK_INT);
	
	if(length && length->ival <= 0)
		fatal_at(length, "array length must be greater than 0");
	
	if(!eat(TK_RBRACK)) {
		if(length)
			fatal_after(last, "expected ]");
		else
			fatal_after(last, "expected integer literal for array length");
	}
	
	Type *itemtype = p_type();
	if(!itemtype)
		fatal_at(last, "expected item type");
	
	return new_array_type(length ? length->ival : -1, itemtype);
}

static Type *p_type()
{
	Type *dtype = 0;
	(dtype = p_primtype()) ||
	(dtype = p_nametype()) ||
	(dtype = p_ptrtype()) ||
	(dtype = p_arraytype()) ;
	return dtype;
}

static Type *complete_type(Type *dtype, Expr *expr)
{
	// automatic array length completion from expr
	for(
		Type *dt = dtype, *st = expr->dtype;
		dt->kind == ARRAY && st->kind == ARRAY;
		dt = dt->itemtype, st = st->itemtype
	) {
		if(dt->length == -1) {
			dt->length = st->length;
		}
	}
}

/*
	Might modify expr and dtype
*/
Expr *cast_expr(Expr *expr, Type *dtype, int explicit)
{
	Type *stype = expr->dtype;
	
	// can not cast from none type
	if(stype->kind == NONE)
		fatal_at(expr->start, "expression has no value");
	
	// types equal => no cast needed
	if(type_equ(stype, dtype))
		return expr;
	
	// integral types are castable into other integral types
	if(is_integral_type(stype) && is_integral_type(dtype))
		return eval_integral_cast(expr, dtype);
	
	// one pointer to some other by explicit cast always ok
	if(explicit && stype->kind == PTR && dtype->kind == PTR)
		return new_cast_expr(expr, dtype);
	
	// array ptr to dynamic array ptr when same item type
	if(
		stype->kind == PTR && stype->subtype->kind == ARRAY &&
		is_dynarray_ptr_type(dtype) &&
		type_equ(stype->subtype->itemtype, dtype->subtype->itemtype)
	) {
		return new_cast_expr(expr, dtype);
	}
	
	// arrays with equal length
	if(
		stype->kind == ARRAY && dtype->kind == ARRAY &&
		stype->length == dtype->length
	) {
		// array literal => cast each item to itemtype
		if(expr->kind == ARRAY) {
			for(
				Expr *prev = 0, *item = expr->exprs;
				item;
				prev = item, item = item->next
			) {
				Expr *new_item = cast_expr(item, dtype->itemtype, explicit);
				if(prev) prev->next = new_item;
				else expr->exprs = new_item;
				new_item->next = item->next;
				item = new_item;
			}
			stype->itemtype = dtype->itemtype;
			return expr;
		}
		// no array literal => create new array literal with cast items
		else {
			Expr *first = 0;
			Expr *last = 0;
			for(int64_t i=0; i < stype->length; i++) {
				Expr *index = new_int_expr(i, expr->start);
				Expr *subscript = new_subscript(expr, index);
				Expr *item = cast_expr(subscript, dtype->itemtype, explicit);
				headless_list_push(first, last, next, item);
			}
			return new_array_expr(
				first, stype->length, expr->isconst,
				dtype->itemtype, expr->start
			);
		}
	}
	
	fatal_at(
		expr->start,
		"can not convert type  %y  to  %y  (%s)",
		stype, dtype, explicit ? "explicit" : "implicit"
	);
}

static Expr *p_var()
{
	if(!eat(TK_IDENT)) return 0;
	Token *ident = last;
	Decl *decl = lookup(ident->id);
	
	if(!decl)
		fatal_at(last, "name %t not declared", ident);
	
	if(decl->kind == STRUCT)
		fatal_at(last, "%t is the name of a structure", ident);
	
	if(decl->kind == FUNC)
		return new_var_expr(ident->id, new_func_type(decl->dtype), ident);
	
	return new_var_expr(ident->id, decl->dtype, ident);
}

static Expr *p_array()
{
	if(!eat(TK_LBRACK)) return 0;
	
	Token *start = last;
	Expr *first = 0;
	Expr *last_expr = 0;
	Type *itemtype = 0;
	uint64_t length = 0;
	int isconst = 1;
	
	while(1) {
		Expr *item = p_expr();
		if(!item) break;
		isconst = isconst && item->isconst;
		if(first) {
			if(!type_equ(itemtype, item->dtype)) {
				item = cast_expr(item, itemtype, 0);
			}
		}
		else {
			itemtype = item->dtype;
		}
		headless_list_push(first, last_expr, next, item);
		length ++;
		if(!eat(TK_COMMA)) break;
	}
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected comma or ]");
	
	if(length == 0)
		fatal_at(last, "empty array literal is not allowed");
	
	return new_array_expr(first, length, isconst, itemtype, start);
}

static Expr *p_atom()
{
	if(eat(TK_INT))
		return new_int_expr(last->ival, last);
	
	if(eat(TK_false) || eat(TK_true))
		return new_bool_expr(last->type == TK_true, last);
	
	if(eat(TK_STRING))
		return new_string_expr(last->string, last->string_length, last);
		
	if(eat(TK_LPAREN)) {
		Token *start = last;
		Expr *expr = p_expr();
		expr->start = start;
		if(!eat(TK_RPAREN)) fatal_after(last, "expected )");
		return expr;
	}
	
	Expr *expr = 0;
	(expr = p_var()) ||
	(expr = p_array()) ;
	return expr;
}

static Expr *p_cast_x(Expr *expr)
{
	if(!eat(TK_as)) return 0;
	
	Type *dtype = p_type();
	if(!dtype)
		fatal_after(last, "expected type after as");
	
	return cast_expr(expr, dtype, 1);
}

static Expr *p_call_x(Expr *expr)
{
	if(!eat(TK_LPAREN)) return 0;
	
	if(expr->dtype->kind != FUNC)
		fatal_at(expr->start, "not a function you are calling");
	
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after (");
	
	Expr *call = new_expr(CALL, expr->start);
	call->callee = expr;
	call->isconst = 0;
	call->islvalue = 0;
	call->dtype = expr->dtype->returntype;
	return call;
}

static Expr *p_subscript_x(Expr *expr)
{
	if(!eat(TK_LBRACK)) return 0;
	
	while(expr->dtype->kind == PTR) {
		expr = new_deref_expr(expr);
	}
	
	if(expr->dtype->kind != ARRAY) {
		fatal_after(last, "need array to subscript");
	}
	
	Expr *index = p_expr();
	if(!index)
		fatal_after(last, "expected index expression after [");
	
	if(!is_integral_type(index->dtype))
		fatal_at(index->start, "index is not integral");
	
	if(
		expr->dtype->kind == ARRAY &&
		expr->dtype->length >= 0 &&
		index->isconst
	) {
		if(index->ival < 0 || index->ival >= expr->dtype->length)
			fatal_at(
				index->start,
				"index is out of range, must be between 0 .. %u",
				expr->dtype->length - 1
			);
	}
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected ] after index expression");
	
	return eval_subscript(expr, index);
}

static Expr *p_member_x(Expr *expr)
{
	if(!eat(TK_PERIOD)) return 0;
	
	while(expr->dtype->kind == PTR) {
		expr = new_deref_expr(expr);
	}
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_at(last, "expected id of member to access");
		
	Type *dtype = expr->dtype;
	
	if(dtype->kind == ARRAY && token_text_equals(ident, "length"))
		return new_member_expr(expr, ident->id, new_type(INT64));
	
	if(dtype->kind != STRUCT) {
		printf("kind %i\n", dtype->kind);
		fatal_at(expr->start, "no instance to get member");
	}
	
	Decl *struct_decl = dtype->typedecl;
	Scope *struct_scope = struct_decl->body->scope;
	Decl *decl = lookup_flat_in(ident->id, struct_scope);
	
	if(!decl)
		fatal_at(ident, "name %t not declared in struct", ident);
	
	return new_member_expr(expr, ident->id, decl->dtype);
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
	
	Expr *subexpr = p_prefix();
	
	if(!subexpr)
		fatal_after(last, "expected expression to point to");
	
	if(!subexpr->islvalue)
		fatal_at(subexpr->start, "target is not addressable");
	
	if(subexpr->kind == DEREF)
		return subexpr->subexpr;
	
	return new_ptr_expr(subexpr);
}

static Expr *p_deref()
{
	if(!eat(TK_LOWER)) return 0;
	
	Expr *subexpr = p_prefix();
	
	if(!subexpr)
		fatal_at(last, "expected expression to dereference");
		
	if(subexpr->dtype->kind != PTR)
		fatal_at(subexpr->start, "expected pointer to dereference");
	
	if(subexpr->kind == PTR)
		return subexpr->subexpr;
	
	return new_deref_expr(subexpr);
}

static Expr *p_prefix()
{
	Expr *expr = 0;
	(expr = p_ptr()) ||
	(expr = p_deref()) ||
	(expr = p_postfix()) ;
	return expr;
}

static Token *p_operator(int level)
{
	Token *op = 0;
	switch(level) {
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
			(op = eat(TK_MINUS)) ;
			break;
		case OL_MUL:
			(op = eat(TK_MUL)) ||
			(op = eat(TK_DSLASH)) ||
			(op = eat(TK_MOD)) ;
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
		
		Expr *expr = new_expr(BINOP, left->start);
		expr->left = left;
		expr->right = right;
		expr->operator = operator;
		expr->isconst = left->isconst && right->isconst;
		expr->islvalue = 0;
		Type *ltype = left->dtype;
		Type *rtype = right->dtype;
		
		if(is_integral_type(ltype) && is_integral_type(rtype)) {
			if(level == OL_CMP) {
				expr->dtype = new_type(BOOL);
				expr->right = cast_expr(expr->right, ltype, 0);
			}
			else {
				expr->dtype = new_type(INT64);
				expr->left = cast_expr(expr->left, expr->dtype, 0);
				expr->right = cast_expr(expr->right, expr->dtype, 0);
			}
		}
		else if(
			ltype->kind == STRING && rtype->kind == STRING &&
			operator->type == TK_EQUALS
		) {
			expr->dtype = new_type(BOOL);
		}
		else {
			fatal_at(
				expr->operator,
				"can not use types  %y  and  %y  with operator %t",
				ltype, rtype, expr->operator
			);
		}
		
		expr = eval_binop(expr);
		left = expr;
	}
	
	return left;
}

static Expr *p_expr()
{
	return p_binop(0);
}

Expr *p_expr_pub(ParseState *state)
{
	unpack_state(state);
	Expr *expr = p_expr();
	pack_state(state);
	return expr;
}





