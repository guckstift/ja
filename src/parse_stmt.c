#include <stdio.h>
#include "build.h"
#include "utils.h"

#include "parse_utils.h"

static Block *p_block(Scope *scope);

static Expr *p_expr()
{
	ParseState state;
	pack_state(&state);
	Expr *expr = p_expr_pub(&state);
	unpack_state(&state);
	return expr;
}

static Type *p_type()
{
	ParseState state;
	pack_state(&state);
	Type *type = p_type_pub(&state);
	unpack_state(&state);
	return type;
}

static int declare(Decl *decl)
{
	return declare_in(decl, scope);
}

static int redeclare(Decl *decl)
{
	return redeclare_in(decl, scope);
}

static void enter()
{
	scope = new_scope(unit_id, scope);
}

static Scope *leave()
{
	Scope *old_scope = scope;
	scope = scope->parent;
	return old_scope;
}

static Scope *reenter(Scope *new_scope)
{
	scope = new_scope;
}

static Stmt *p_print()
{
	if(!eat(TK_print)) return 0;
	Token *start = last;
	
	Expr *expr = p_expr();
	if(!expr) fatal_after(last, "expected expression to print");
	
	if(
		!is_integral_type(expr->type) &&
		expr->type->kind != PTR && expr->type->kind != STRING
	) {
		fatal_at(expr->start, "can only print numbers, strings or pointers");
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after print statement");
	
	return (Stmt*)new_print(start, scope, expr);
}

static Stmt *p_vardecl_core(Token *start, int exported, int param)
{
	Token *ident = eat(TK_IDENT);
	if(!ident) return 0;
	if(!start) start = ident;
	
	Type *type = 0;
	if(eat(TK_COLON)) {
		type = p_type();
		if(!type) fatal_after(last, "expected type after colon");
	}
	
	Expr *init = 0;
	if(!param && eat(TK_ASSIGN)) {
		init = p_expr();
		if(!init) fatal_after(last, "expected initializer after =");
		
		Token *init_start = init->start;
		
		if(init->type->kind == FUNC)
			fatal_at(init_start, "can not use function as value");
		
		if(init->type->kind == NONE)
			fatal_at(init_start, "expression has no value");
	
		if(scope->structhost && !init->isconst)
			fatal_at(
				init_start,
				"structure members can only be initialized "
				"with constant values"
			);
		
		if(type == 0)
			type = init->type;
		
		complete_type(type, init);
		init = cast_expr(init, type, 0);
	}
	
	if(type == 0)
		fatal_at(
			ident, "%s without type declared",
			param ? "parameter" : "variable"
		);
	
	if(!is_complete_type(type))
		fatal_at(
			ident, "%s with incomplete type  %y  declared",
			param ? "parameter" : "variable", type
		);
	
	if(exported) {
		make_type_exportable(type);
	}
	
	Decl *decl = new_var(start, scope, ident->id, exported, type, init);
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	return (Stmt*)decl;
}

static Stmt *p_vardecl(int exported)
{
	if(!eat(TK_var)) return 0;
	Token *start = last;
	
	Stmt *core = p_vardecl_core(start, exported, 0);
	if(!core) fatal_after(last, "expected identifier after keyword var");
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after variable declaration");
	
	return core;
}

static Stmt *p_funcdecl(int exported)
{
	if(!eat(TK_function)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "functions can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_after(last, "expected identifier after keyword function");
	
	if(!eat(TK_LPAREN))
		fatal_after(last, "expected ( after function name");
	
	Decl **params = 0;
	enter();
	
	while(1) {
		Decl *param = &p_vardecl_core(0, 0, 1)->as_decl;
		if(!param) break;
		array_push(params, param);
		if(exported) make_type_exportable(param->type);
		if(!eat(TK_COMMA)) break;
	}
	
	Scope *func_scope = leave();
	
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after parameter list");
	
	Type *returntype = new_type(NONE);
	
	if(eat(TK_COLON)) {
		returntype = p_type();
		if(!returntype)
			fatal_after(last, "expected return type after colon");
		
		if(!is_complete_type(returntype))
			fatal_at(
				ident, "function with incomplete type  %y  declared",
				returntype
			);
		
		if(exported) {
			make_type_exportable(returntype);
		}
	}
	
	Decl *decl = new_func(
		start, scope, ident->id, exported, returntype, params
	);
	
	func_scope->funchost = decl;
	
	if(eat(TK_SEMICOLON)) {
		decl->isproto = 1;
	}
	else if(eat(TK_LCURLY)) {
		decl->isproto = 0;
	}
	else {
		fatal_after(last, "expected { or ; after function head");
	}
	
	Decl *existing = lookup(decl->id);
	
	if(existing) {
		if(existing->kind != FUNC)
			fatal_at(ident, "name %t already declared", ident);
		
		if(existing->isproto == 0)
			fatal_at(ident, "function %t already implemented", ident);
		
		if(!type_equ(existing->type, decl->type)) {
			fatal_at(start,
				"function prototype had a different signature  %y "
				", now it is  %y",
				existing->type, decl->type
			);
		}
		
		if(decl->exported != existing->exported)
			fatal_at(
				ident, "function %t was already declared as %s now it %s",
				ident,
				existing->exported ? "exported" : "not exported",
				decl->exported ? "is" : "is not"
			);
		
		redeclare(decl);
	}
	else {
		declare(decl);
	}
	
	if(decl->isproto == 0) {
		reenter(func_scope);
		decl->body = p_block(0);
		leave();
		
		if(!eat(TK_RCURLY))
			fatal_after(last, "expected } after function body");
	}
	
	return (Stmt*)decl;
}

static Stmt *p_structdecl(int exported)
{
	if(!eat(TK_struct)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "structures can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_after(last, "expected identifier after keyword struct");
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	Decl **members = 0;
	enter();
	
	while(1) {
		Decl *member = &p_vardecl(0)->as_decl;
		if(!member) break;
		array_push(members, member);
		if(exported) make_type_exportable(member->type);
	}
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after structure body");
	
	if(array_length(members) == 0)
		fatal_at(start, "empty structure");
	
	Scope *struct_scope = leave();
	Decl *decl = new_struct(start, scope, ident->id, exported, members);
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	struct_scope->structhost = decl;
	return (Stmt*)decl;
}

static Stmt *p_ifstmt()
{
	if(!eat(TK_if)) return 0;
	Token *start = last;
	
	Expr *cond = p_expr();
	if(!cond)
		fatal_at(last, "expected condition after if");
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after condition");
	
	Block *if_body = p_block(0);
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after if-body");
	
	Block *else_body = 0;
	if(eat(TK_else)) {
		if(match(TK_if)){
			Stmt *else_if = p_ifstmt();
			Stmt **stmts = 0;
			array_push(stmts, else_if);
			else_body = new_block(stmts, scope);
		}
		else {
			if(!eat(TK_LCURLY))
				fatal_after(last, "expected { after else");
			
			else_body = p_block(0);
			
			if(!eat(TK_RCURLY))
				fatal_after(last, "expected } after else-body");
		}
	}
	
	return (Stmt*)new_if(start, cond, if_body, else_body);
}

static Stmt *p_whilestmt()
{
	if(!eat(TK_while)) return 0;
	Token *start = last;
	
	Expr *cond = p_expr();
	if(!cond)
		fatal_at(last, "expected condition after while");
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after condition");
	
	Block *body = p_block(0);
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after if-body");
	
	return (Stmt*)new_while(start, cond, body);
}

static Stmt *p_returnstmt()
{
	if(!eat(TK_return)) return 0;
	Token *start = last;
	Decl *funchost = scope->funchost;
	
	if(!funchost)
		fatal_at(last, "return outside of any function");
	
	Expr *result = p_expr();
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after return statement");
	
	Type *functype = funchost->type;
	Type *returntype = functype->returntype;
	
	if(returntype->kind == NONE) {
		if(result) {
			fatal_at(result->start, "function should not return values");
		}
	}
	else if(!result) {
		fatal_after(start, "expected expression to return");
	}
	else {
		result = cast_expr(result, returntype, 0);
	}
	
	return (Stmt*)new_return(start, scope, result);
}

static Stmt *p_import()
{
	if(!eat(TK_import)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "imports can only be used at top level");
	
	Token *filename = eat(TK_STRING);
	Token **idents = 0;
	
	if(!filename) {
		while(1) {
			Token *ident = eat(TK_IDENT);
			if(!ident) break;
			array_push(idents, ident);
			if(!eat(TK_COMMA)) break;
		}
		
		if(idents == 0) {
			fatal_after(
				start, "expected identifier list or filename string to import"
			);
		}
		
		if(!eat(TK_from))
			error_at(cur, "expected 'from' after identifier list");
		
		filename = eat(TK_STRING);
		if(!filename)
			fatal_at(cur, "expected filename string to import from");
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after import statement");
	
	ParseState state;
	pack_state(&state);
	Unit *unit = import(filename->string);
	unpack_state(&state);
	
	array_for(scope->imports, i) {
		if(scope->imports[i]->unit == unit) {
			fatal_at(start, "unit already imported");
		}
	}
	
	Scope *unit_scope = unit->block->scope;
	Decl **decls = 0;
	
	array_for(idents, i) {
		Token *ident = idents[i];
		Decl *decl = lookup_in(ident->id, unit_scope);
		
		if(decl == 0 || decl->exported == 0) {
			fatal_at(ident, "no exported symbol %t in unit", ident);
		}
		
		if(!declare(decl)) {
			fatal_at(ident, "name %t already declared", ident);
		}
		
		array_push(decls, decl);
	}
	
	Import *import = new_import(start, scope, unit, decls);
	array_push(scope->imports, import);
	return (Stmt*)import;
}

static Stmt *p_export()
{
	if(!eat(TK_export)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "exports can only be done at top level");
	
	Stmt *stmt = 0;
	(stmt = p_vardecl(1)) ||
	(stmt = p_funcdecl(1)) ||
	(stmt = p_structdecl(1)) ;
	
	if(!stmt) {
		fatal_at(
			cur, "you can only export variables, structures or functions"
		);
	}
	
	stmt->start = start;
	return stmt;
}

static Stmt *p_assign()
{
	Expr *target = p_expr();
	if(!target) return 0;
	
	if(target->kind == CALL && eat(TK_SEMICOLON)) {
		return (Stmt*)new_call(scope, target);
	}
	
	if(!target->islvalue)
		fatal_at(target->start, "left side is not assignable");
	
	if(!eat(TK_ASSIGN))
		fatal_after(last, "expected = after left side");
	
	Expr *expr = p_expr();
	
	if(!expr)
		fatal_at(last, "expected right side after =");
		
	expr = cast_expr(expr, target->type, 0);
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after assignment");
	
	return (Stmt*)new_assign(scope, target, expr);
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl(0)) ||
	(stmt = p_funcdecl(0)) ||
	(stmt = p_structdecl(0)) ||
	(stmt = p_ifstmt()) ||
	(stmt = p_whilestmt()) ||
	(stmt = p_returnstmt()) ||
	(stmt = p_import()) ||
	(stmt = p_export()) ||
	(stmt = p_assign()) ;
	return stmt;
}

static Stmt **p_stmts()
{
	Stmt **stmts = 0;
	while(1) {
		Stmt *stmt = p_stmt();
		if(!stmt) break;
		array_push(stmts, stmt);
	}
	
	return stmts;
}

static Block *p_block(Scope *scope)
{
	if(scope) reenter(scope);
	else enter();
	Stmt **stmts = p_stmts();
	Scope *block_scope = leave();
	return new_block(stmts, block_scope);
}

Block *p_block_pub(ParseState *state, Scope *scope)
{
	unpack_state(state);
	Block *block = p_block(scope);
	pack_state(state);
	return block;
}
