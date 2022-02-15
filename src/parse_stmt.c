#include <stdio.h>
#include "build.h"
#include "utils.h"

#include "parse_utils.h"

static Stmt *p_stmts(Decl *func);

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

static void declare_builtins()
{
	Type *dynarray_string_type = new_ptr_type(
		new_array_type(-1, new_type(STRING))
	);
	
	Token *argv_id = create_id("argv", 0);
	
	Decl *argv = new_vardecl(
		argv_id, dynarray_string_type,
		0, 0, scope
	);
	
	argv->builtin = 1;
	
	declare(argv);
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

static Stmt *p_print()
{
	if(!eat(TK_print)) return 0;
	Print *print = (Print*)new_stmt(PRINT, last, scope);
	print->expr = p_expr();
	
	if(!print->expr)
		fatal_after(last, "expected expression to print");
	
	if(
		!is_integral_type(print->expr->dtype) &&
		print->expr->dtype->kind != PTR &&
		print->expr->dtype->kind != STRING
	) {
		fatal_at(print->expr->start, "can only print numbers or pointers");
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after print statement");
	
	return (Stmt*)print;
}

static Stmt *p_vardecl(int exported)
{
	Token *start = eat(TK_var);
	if(!start) return 0;
	
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_after(last, "expected identifier after keyword var");
	
	Type *dtype = 0;
	if(eat(TK_COLON)) {
		dtype = p_type();
		if(!dtype) fatal_after(last, "expected type after colon");
	}
	
	Expr *init = 0;
	if(eat(TK_ASSIGN)) {
		init = p_expr();
		if(!init) fatal_after(last, "expected initializer after =");
		
		Token *init_start = init->start;
		
		if(init->dtype->kind == FUNC)
			fatal_at(init_start, "can not use function as value");
		
		if(init->dtype->kind == NONE)
			fatal_at(init_start, "expression has no value");
	
		if(scope->struc && !init->isconst)
			fatal_at(
				init_start,
				"structure members can only be initialized "
				"with constant values"
			);
		
		if(dtype == 0)
			dtype = init->dtype;
		
		complete_type(dtype, init);
		init = cast_expr(init, dtype, 0);
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after variable declaration");
	
	if(dtype == 0)
		fatal_at(ident, "variable without type declared");
	
	if(!is_complete_type(dtype))
		fatal_at(ident, "variable with incomplete type  %y  declared", dtype);
	
	if(exported) {
		make_type_exportable(dtype);
	}
	
	Decl *decl = new_vardecl(ident->id, dtype, init, start, scope);
	decl->exported = exported;
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	return (Stmt*)decl;
}

static Stmt *p_vardecls(Decl *struc)
{
	enter();
	if(struc) scope->struc = struc;
	
	Stmt *first_decl = 0;
	Stmt *last_decl = 0;
	while(1) {
		Stmt *decl = p_vardecl(0);
		if(!decl) break;
		headless_list_push(first_decl, last_decl, next, decl);
	}
	
	leave();
	return first_decl;
}

static Stmt *p_funcdecl(int exported)
{
	Token *start = eat(TK_function);
	if(!start) return 0;
	
	if(scope->parent)
		fatal_at(last, "functions can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_after(last, "expected identifier after keyword function");
	
	if(!eat(TK_LPAREN))
		fatal_after(last, "expected ( after function name");
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after (");
	
	Type *dtype = new_type(NONE);
	if(eat(TK_COLON)) {
		dtype = p_type();
		if(!dtype) fatal_after(last, "expected return type after colon");
		
		if(!is_complete_type(dtype))
			fatal_at(
				ident, "function with incomplete type  %y  declared", dtype
			);
		
		if(exported) {
			make_type_exportable(dtype);
		}
	}
	
	Decl *decl = (Decl*)new_stmt(FUNC, start, scope);
	decl->exported = exported;
	decl->id = ident->id;
	decl->dtype = dtype;
	decl->next_decl = 0;
	
	if(!declare(decl)) {
		Decl *existing_decl = lookup(decl->id);
		
		if(existing_decl->kind == FUNC && existing_decl->isproto) {
			if(!type_equ(existing_decl->dtype, decl->dtype)) {
				fatal_at(ident,
					"function prototype had a different return type  %y "
					", now it is  %y",
					existing_decl->dtype,
					decl->dtype
				);
			}
			
			decl = existing_decl;
		}
		else {
			fatal_at(ident, "name %t already declared", ident);
		}
	}
	
	if(eat(TK_SEMICOLON)) {
		decl->isproto = 1;
	}
	else {
		if(!eat(TK_LCURLY))
			fatal_after(last, "expected {");
		
		decl->body = p_stmts(decl);
		
		if(!eat(TK_RCURLY))
			fatal_after(last, "expected } after function body");
		
		decl->isproto = 0;
	}
	
	return (Stmt*)decl;
}

static Stmt *p_structdecl(int exported)
{
	Token *start = eat(TK_struct);
	if(!start) return 0;
	
	if(scope->parent)
		fatal_at(last, "structures can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_after(last, "expected identifier after keyword struct");
	
	Decl *decl = (Decl*)new_stmt(STRUCT, start, scope);
	decl->exported = exported;
	decl->id = ident->id;
	decl->next_decl = 0;
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	decl->body = p_vardecls(decl);
	if(decl->body == 0)
		fatal_at(decl->start, "empty structure");
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after function body");
	
	return (Stmt*)decl;
}

static Stmt *p_ifstmt()
{
	if(!eat(TK_if)) return 0;
	If *stmt = (If*)new_stmt(IF, last, scope);
	stmt->expr = p_expr();
	if(!stmt->expr)
		fatal_at(last, "expected condition after if");
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after condition");
	stmt->if_body = p_stmts(0);
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after if-body");
		
	if(eat(TK_else)) {
		if(match(TK_if)){
			stmt->else_body = p_ifstmt();
		}
		else {
			if(!eat(TK_LCURLY))
				fatal_after(last, "expected { after else");
			stmt->else_body = p_stmts(0);
			if(!eat(TK_RCURLY))
				fatal_after(last, "expected } after else-body");
		}
	}
	else {
		stmt->else_body = 0;
	}
	
	return (Stmt*)stmt;
}

static Stmt *p_whilestmt()
{
	if(!eat(TK_while)) return 0;
	While *stmt = (While*)new_stmt(WHILE, last, scope);
	stmt->expr = p_expr();
	if(!stmt->expr)
		fatal_at(last, "expected condition after while");
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after condition");
	stmt->while_body = p_stmts(0);
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after while-body");
	return (Stmt*)stmt;
}

static Stmt *p_returnstmt()
{
	if(!eat(TK_return)) return 0;
	Decl *func = scope->func;
	
	if(!func)
		fatal_at(last, "return outside of any function");
	
	Type *dtype = func->dtype;
	Return *stmt = (Return*)new_stmt(RETURN, last, scope);
	stmt->expr = p_expr();
	
	if(dtype->kind == NONE) {
		if(stmt->expr)
			fatal_at(stmt->expr->start, "function should not return values");
	}
	else {
		if(!stmt->expr)
			fatal_after(last, "expected expression to return");
		stmt->expr = cast_expr(stmt->expr, dtype, 0);
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after return statement");
	
	return (Stmt*)stmt;
}

static Stmt *p_import()
{
	Token *start = cur;
	if(!eat(TK_import)) return 0;
	
	if(scope->parent)
		fatal_at(last, "imports can only be used at top level");
	
	Token *filename = eat(TK_STRING);
	Token *first_ident = 0;
	int64_t ident_count = 0;
	
	if(!filename) {
		while(1) {
			Token *ident = eat(TK_IDENT);
			if(!ident) break;
			ident_count ++;
			if(!first_ident) first_ident = ident;
			if(!eat(TK_COMMA)) break;
		}
		
		if(!first_ident) {
			fatal_after(
				start,
				"expected identifier list or filename string to import"
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
	
	Unit *unit = import(filename->string);
	
	for_list(Import, import, scope->first_import, next_import) {
		if(unit == import->unit)
			fatal_at(start, "unit already imported");
	}
	
	Token *ident = first_ident;
	
	for(int64_t i=0; i < ident_count; i++) {
		Stmt *unit_stmts = unit->stmts;
		Decl *found_decl = 0;
		
		if(unit_stmts) {
			Scope *unit_scope = unit_stmts->scope;
			Decl *decl = lookup_in(ident->id, unit_scope);
			
			if(decl && decl->exported) {
				found_decl = decl;
			}
		}
		
		if(!found_decl)
			fatal_at(ident, "could not find symbol %t in unit", ident);
		
		if(!declare(found_decl))
			fatal_at(ident, "name %t already declared", ident);
		
		ident += 2; // move to next ident (skip comma token)
	}
	
	Import *import = new_import(filename, unit, start, scope);
	import->imported_idents = first_ident;
	import->imported_ident_count = ident_count;
	list_push(scope, first_import, last_import, next_import, import);
	
	return (Stmt*)import;
}

static Stmt *p_export()
{
	if(!eat(TK_export)) return 0;
	
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
	
	return stmt;
}

static Stmt *p_assign()
{
	Expr *target = p_expr();
	if(!target) return 0;
	
	if(target->kind == CALL && eat(TK_SEMICOLON)) {
		Call *call = (Call*)new_stmt(CALL, target->start, scope);
		call->call = target;
		return (Stmt*)call;
	}
	
	if(!target->islvalue)
		fatal_at(target->start, "left side is not assignable");
	Assign *stmt = (Assign*)new_stmt(ASSIGN, target->start, scope);
	stmt->target = target;
	if(!eat(TK_ASSIGN))
		fatal_after(last, "expected = after left side");
	stmt->expr = p_expr();
	if(!stmt->expr)
		fatal_at(last, "expected right side after =");
	stmt->expr = cast_expr(stmt->expr, stmt->target->dtype, 0);
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after variable declaration");
	return (Stmt*)stmt;
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

static Stmt *p_stmts(Decl *func)
{
	enter();
	if(func) scope->func = func;
	
	if(!scope->parent) {
		declare_builtins();
	}
	
	Stmt *first_stmt = 0;
	Stmt *last_stmt = 0;
	while(1) {
		Stmt *stmt = p_stmt();
		if(!stmt) break;
		if(stmt->kind == FUNC && stmt->as_decl.isproto == 1) continue;
		headless_list_push(first_stmt, last_stmt, next, stmt);
	}
	
	leave();
	return first_stmt;
}

Stmt *p_stmts_pub(ParseState *state, Decl *func)
{
	unpack_state(state);
	Stmt *stmt = p_stmts(func);
	pack_state(state);
	return stmt;
}
