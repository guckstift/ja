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

static Expr *p_expr()
{
	ParseState state;
	pack_state(&state);
	Expr *expr = p_expr_pub(&state);
	unpack_state(&state);
	return expr;
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

static void make_type_exportable(Type *dtype)
{
	while(dtype->kind == PTR || dtype->kind == ARRAY) {
		dtype = dtype->subtype;
	}
	
	if(dtype->kind == STRUCT && dtype->typedecl->exported != 1) {
		dtype->typedecl->exported = 1;
		
		for_list(Stmt, member, dtype->typedecl->body, next) {
			make_type_exportable(member->as_decl.dtype);
		}
	}
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

Stmt *parse(Tokens *tokens, char *_unit_id)
{
	// save states
	Token *old_cur = cur;
	Token *old_last = last;
	char *old_src_end = src_end;
	Scope *old_scope = scope;
	char *old_unit_id = unit_id;
	
	cur = tokens->first;
	last = 0;
	src_end = tokens->last->start + tokens->last->length;
	scope = 0;
	unit_id = _unit_id;
	Stmt *stmts = p_stmts(0);
	if(!eat(TK_EOF))
		fatal_at(cur, "invalid statement");
	
	// restore states
	cur = old_cur;
	last = old_last;
	src_end = old_src_end;
	scope = old_scope;
	unit_id = old_unit_id;
	
	return stmts;
}
