#include <stdbool.h>
#include "analyze.h"
#include "array.h"
#include "parse_internal.h"

#include <stdio.h>

static bool repeat_analyze = false;

static void a_block(Block *block);
static void a_expr(Expr *expr);

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

static void a_call(Expr *expr)
{
	a_expr(expr->callee);
}

static void a_expr(Expr *expr)
{
	switch(expr->kind) {
		case VAR:
			a_var(expr);
			break;
		case CALL:
			a_call(expr);
			break;
	}
}

static void a_stmt(Stmt *stmt)
{
	switch(stmt->kind) {
		case PRINT:
			a_expr(stmt->as_print.expr);
			break;
		case VAR:
			if(stmt->as_decl.init)
				a_expr(stmt->as_decl.init);
			
			break;
		case FUNC:
			a_block(stmt->as_decl.body);
			
			if(stmt->as_decl.deps_scanned == 0) {
				stmt->as_decl.deps_scanned = 1;
				repeat_analyze = true;
			}
			
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
}

void analyze(Unit *unit)
{
	src_end = unit->src + unit->src_len;
	a_block(unit->block);
	
	if(repeat_analyze) {
		repeat_analyze = false;
		analyze(unit);
	}
}
