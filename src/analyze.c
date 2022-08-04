#include <stdbool.h>
#include "analyze.h"
#include "array.h"
#include "parse_internal.h"

#include <stdio.h>

static bool repeat_analyze = false;

static void a_block(Block *block);
static void a_expr(Expr *expr);

static Expr *eval_integral_cast(Expr *expr, Type *type)
{
	expr->type = type;
	expr->kind = INT;
	
	switch(type->kind) {
		case BOOL:
			expr->kind = BOOL;
			expr->value = expr->value != 0;
			break;
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
	}
	
	return expr;
}

/*
	Might modify expr
*/
static Expr *adjust_expr_to_type(Expr *expr, Type *type, bool explicit)
{
	Type *expr_type = expr->type;
	
	// types equal => no adjustment needed
	if(type_equ(expr_type, type))
		return expr;
	
	// integral types are castable amongst themselves
	if(is_integral_type(expr_type) && is_integral_type(type)) {
		if(expr->isconst) {
			return eval_integral_cast(expr, type);
		}
		
		return new_cast_expr(expr, type);
	}
	
	// one pointer to some other by explicit cast always ok
	if(explicit && expr_type->kind == PTR && type->kind == PTR)
		return expr; // no change needed, expr is the subexpr of a cast expr
	
	// array literal with matching length
	if(
		expr->kind == ARRAY && type->kind == ARRAY &&
		expr_type->length == type->length
	) {
		array_for(expr->items, i) {
			expr->items[i] = adjust_expr_to_type(
				expr->items[i], type->itemtype, false
			);
		}
		
		expr_type->itemtype = type->itemtype;
		return expr;
	}
	
	fatal_at(
		expr->start,
		"can not convert type  %y  to  %y",
		expr_type, type
	);
}

static void a_var(Expr *expr)
{
	Decl *decl = lookup(expr->id);
	
	if(!decl)
		fatal_at(expr->start, "name %t not declared", expr->id);
	
	if(decl->kind == VAR && decl->start > expr->start)
		fatal_at(expr->start, "variable %t declared later", expr->id);
	
	if(decl->kind == VAR && scope->funchost) {
		Decl *func = scope->funchost;
		
		if(func->deps_scanned == 0) {
			Scope *func_scope = func->body->scope;
			Scope *var_scope = decl->scope;
			
			if(scope_contains_scope(var_scope, func_scope)) {
				array_push(func->deps, decl);
			}
		}
	}
	
	if(decl->kind == FUNC && decl->deps_scanned) {
		array_for(decl->deps, i) {
			Decl *dep = decl->deps[i];
			
			if(dep->start > expr->start) {
				fatal_at(
					expr->start, "%t uses %t which is declared later",
					expr->id, dep->id
				);
			}
		}
	}
	
	expr->decl = decl;
	expr->type = decl->type;
}

static void a_ptr(Expr *expr)
{
	Expr *subexpr = expr->subexpr;
	a_expr(subexpr);
	expr->type->subtype = subexpr->type;
}

static void a_deref(Expr *expr)
{
	Expr *ptr = expr->ptr;
	a_expr(ptr);
	
	if(ptr->type->kind != PTR)
		fatal_at(ptr->start, "expected pointer to dereference");
	
	expr->type = ptr->type->subtype;
}

static void a_cast(Expr *expr)
{
	a_expr(expr->subexpr);
	expr->subexpr = adjust_expr_to_type(expr->subexpr, expr->type, true);
}

static void a_subscript(Expr *expr)
{
	Expr *array = expr->array;
	Expr *index = expr->index;
	a_expr(array);
	a_expr(index);

	a_expr(expr->subexpr);
	
	while(expr->array->type->kind == PTR) {
		expr->array = new_deref_expr(expr->array->start, expr->array);
		array = expr->array;
	}
	
	if(array->type->kind != ARRAY && array->type->kind != STRING)
		fatal_at(array->start, "need array or string to subscript");
	
	if(!is_integral_type(index->type))
		fatal_at(index->start, "index is not an integer or a boolean");
	
	if(
		array->type->kind == ARRAY && array->type->length >= 0 &&
		index->isconst
	) {
		if(index->value < 0 || index->value >= array->type->length)
			fatal_at(
				index->start,
				"index is out of range, must be between 0 .. %u",
				array->type->length - 1
			);
	}
	
	expr->type = array->type->itemtype;
}

static void a_array(Expr *expr)
{
	Expr **items = expr->items;
	Type *itemtype = 0;
	
	array_for(items, i) {
		a_expr(items[i]);
		
		if(i == 0) {
			itemtype = items[i]->type;
		}
		else {
			items[i] = adjust_expr_to_type(items[i], itemtype, false);
		}
	}
	
	expr->type->itemtype = itemtype;
}

static void a_call(Expr *expr)
{
	Expr *callee = expr->callee;
	a_expr(callee);
	
	if(callee->type->kind != FUNC)
		fatal_at(callee->start, "not a function you are calling");
	
	Expr **args = expr->args;
}

static void a_expr(Expr *expr)
{
	switch(expr->kind) {
		case VAR:
			a_var(expr);
			break;
		case PTR:
			a_ptr(expr);
			break;
		case DEREF:
			a_deref(expr);
			break;
		case CAST:
			a_cast(expr);
			break;
		case SUBSCRIPT:
			a_subscript(expr);
			break;
		case ARRAY:
			a_array(expr);
			break;
		case CALL:
			a_call(expr);
			break;
	}
}

static void a_vardecl(Decl *decl)
{
	if(decl->init) {
		a_expr(decl->init);
		
		if(decl->init->type->kind == FUNC)
			fatal_at(decl->init->start, "can not use a function as value");
		
		if(decl->type == 0)
			decl->type = decl->init->type;
		else
			decl->init = adjust_expr_to_type(decl->init, decl->type, false);
	}
}

static void a_funcdecl(Decl *decl)
{
	a_block(decl->body);
	
	if(decl->deps_scanned == 0) {
		decl->deps_scanned = 1;
		repeat_analyze = true;
	}
}

static void a_assign(Assign *assign)
{
	a_expr(assign->target);
	a_expr(assign->expr);
	
	assign->expr = adjust_expr_to_type(
		assign->expr, assign->target->type, false
	);
}

static void a_stmt(Stmt *stmt)
{
	switch(stmt->kind) {
		case PRINT:
			a_expr(stmt->as_print.expr);
			break;
		case VAR:
			a_vardecl(&stmt->as_decl);
			break;
		case FUNC:
			a_funcdecl(&stmt->as_decl);
			break;
		case ASSIGN:
			a_assign(&stmt->as_assign);
			break;
		case CALL:
			a_call(stmt->as_call.call);
			break;
	}
}

static void a_stmts(Stmt **stmts)
{
	array_for(stmts, i) {
		a_stmt(stmts[i]);
	}
}

static void a_block(Block *block)
{
	scope = block->scope;
	a_stmts(block->stmts);
	scope = scope->parent;
}

void analyze(Unit *unit)
{
	src_end = unit->src + unit->src_len;
	a_block(unit->block);
	
	if(repeat_analyze) {
		repeat_analyze = false;
		printf("repeat analyze\n");
		analyze(unit);
	}
}
