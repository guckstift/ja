#include <stdio.h>
#include "parse_internal.h"
#include "build.h"
#include "array.h"

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

static void declare_builtins()
{
	Token *argv_id = create_id("argv", 0);
	Type *string_slice_type = new_slice_type(new_type(STRING));
	
	Decl *argv = new_var(
		argv_id, scope, argv_id, 0, 0, string_slice_type, 0
	);
	
	argv->builtin = 1;
	
	declare(argv);
}

static void enter()
{
	scope = new_scope(unit_id, scope);
	if(!scope->parent) declare_builtins();
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
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after print statement");
	
	return (Stmt*)new_print(start, scope, expr);
}

static Stmt *p_vardecl_core(Token *start, int exported, int param, int foreign)
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
		if(foreign) {
			fatal_at(
				cur,
				"variables imported from dynamic libraries "
				"can not have initialization"
			);
		}
		
		init = p_expr();
		if(!init) fatal_after(last, "expected initializer after =");
		
		Token *init_start = init->start;
		
		if(scope->structhost && !init->isconst)
			fatal_at(
				init_start,
				"structure members can only be initialized "
				"with constant values"
			);
	}
	
	if(type == 0 && init == 0) {
		fatal_at(
			ident, "%s without type declared",
			param ? "parameter" : "variable"
		);
	}
	
	Decl *decl = new_var(start, scope, ident->id, exported, param, type, init);
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	decl->end = cur;
	return (Stmt*)decl;
}

static Stmt *p_vardecl(int exported, int foreign)
{
	if(!eat(TK_var)) return 0;
	Token *start = last;
	
	Stmt *core = p_vardecl_core(start, exported, 0, foreign);
	if(!core) fatal_after(last, "expected identifier after keyword var");
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after variable declaration");
	
	return core;
}

static Decl *p_funchead(int exported)
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
		Decl *param = &p_vardecl_core(0, 0, 1, 0)->as_decl;
		if(!param) break;
		array_push(params, param);
		if(!eat(TK_COMMA)) break;
	}
	
	Scope *func_scope = leave();
	
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after parameter list");
	
	Type *returntype = new_type(NONE);
	
	if(eat(TK_COLON)) {
		returntype = p_type();
		if(!returntype) fatal_after(last, "expected return type after colon");
	}
	
	Decl *decl = new_func(
		start, scope, ident->id, exported, returntype, params, func_scope
	);
	
	func_scope->funchost = decl;
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	return decl;
}

static Stmt *p_foreign_func(int exported)
{
	Decl *decl = p_funchead(exported);
	if(!decl) return 0;
	
	decl->imported = 1;
	decl->isproto = 1;
	decl->deps_scanned = 1;
	
	if(!eat(TK_SEMICOLON))
		fatal_after(last, "expected ; after function head");
	
	decl->end = cur;
	return (Stmt*)decl;
}

static Stmt *p_funcdecl(int exported)
{
	Decl *decl = p_funchead(exported);
	if(!decl) return 0;
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after function head");
	
	decl->body = p_block(decl->func_scope);
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after function body");
	
	decl->end = cur;
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
		Decl *member = &p_vardecl_core(0, 0, 0, 0)->as_decl;
		if(!member) break;
		
		if(member->init && !member->init->isconst) {
			fatal_at(
				member->init->start,
				"struct members must be constant"
			);
		}
		
		if(!eat(TK_SEMICOLON))
			error_after(last, "expected semicolon after struct member");
		
		array_push(members, member);
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
	decl->end = cur;
	return (Stmt*)decl;
}

static Stmt *p_enumdecl(int exported)
{
	if(!eat(TK_enum)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "enums can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_after(last, "expected identifier after keyword enum");
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	EnumItem **items = 0;
	int64_t num = 0;
	
	while(1) {
		Token *ident = eat(TK_IDENT);
		if(!ident) break;
		
		array_for(items, i) {
			if(items[i]->id == ident->id) {
				fatal_at(ident, "enum item already defined");
			}
		}
		
		EnumItem *item = malloc(sizeof(EnumItem));
		item->id = ident->id;
		item->val = 0;
		
		if(eat(TK_ASSIGN)) {
			Expr *val = p_expr();
			if(!val) fatal_at(last, "expected expression after =");
			
			if(!val->isconst)
				fatal_at(val->start, "expression must be constant");
			
			item->val = val;
		}
		
		array_push(items, item);
		if(!eat(TK_COMMA)) break;
	}
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after enum body");
	
	if(array_length(items) == 0)
		fatal_at(start, "empty enum");
	
	Decl *decl = new_enum(start, scope, ident->id, items, exported);
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	decl->end = cur;
	return (Stmt*)decl;
}

static Stmt *p_uniondecl(int exported)
{
	if(!eat(TK_union)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "unions can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_after(last, "expected identifier after keyword union");
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	Decl **members = 0;
	enter();
	
	while(1) {
		Decl *member = &p_vardecl_core(0, 0, 0, 0)->as_decl;
		if(!member) break;
		
		if(member->init) {
			fatal_at(
				member->init->start,
				"union members can not have default values"
			);
		}
		
		if(!eat(TK_SEMICOLON))
			error_after(last, "expected semicolon after union member");
		
		array_push(members, member);
	}
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after union body");
	
	if(array_length(members) == 0)
		fatal_at(start, "empty union");
	
	Scope *struct_scope = leave();
	Decl *decl = new_union(start, scope, ident->id, exported, members);
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	struct_scope->structhost = decl;
	decl->end = cur;
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
		if(match(TK_if)) {
			enter();
			Stmt *else_if = p_ifstmt();
			Stmt **stmts = 0;
			array_push(stmts, else_if);
			Scope *block_scope = leave();
			else_body = new_block(stmts, block_scope);
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
	
	enter();
	Scope *blockscope = leave();
	While *stmt = new_while(start, blockscope, cond, 0);
	blockscope->loophost = (Stmt*)stmt;
	stmt->body = p_block(blockscope);
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after if-body");
	
	return (Stmt*)stmt;
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

static Stmt *p_foreign()
{
	if(!eat(TK_foreign)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "foreign imports can only be used at top level");
	
	Token *filename = eat(TK_STRING);
	if(!filename) fatal_at(cur, "expected filename of library to import from");
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after library name");
	
	Decl **decls = 0;
	
	while(1) {
		Decl *decl = 0;
		(decl = &p_foreign_func(0)->as_decl) ||
		(decl = &p_vardecl(0, 1)->as_decl) ;
		if(!decl) break;
		array_push(decls, decl);
	}
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after library list");
	
	Foreign *import = new_foreign(start, scope, filename->string, decls);
	array_push(scope->foreigns, import);
	return (Stmt*)import;
}

static Stmt *p_export()
{
	if(!eat(TK_export)) return 0;
	Token *start = last;
	
	if(scope->parent)
		fatal_at(last, "exports can only be done at top level");
	
	Stmt *stmt = 0;
	(stmt = p_vardecl(1, 0)) ||
	(stmt = p_funcdecl(1)) ||
	(stmt = p_structdecl(1)) ||
	(stmt = p_enumdecl(1)) ;
	
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
	
	if(!eat(TK_ASSIGN))
		fatal_after(last, "expected = after left side");
	
	Expr *expr = p_expr();
	
	if(!expr)
		fatal_at(last, "expected right side after =");
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after assignment");
	
	return (Stmt*)new_assign(scope, target, expr);
}

static Stmt *p_break()
{
	if(!eat(TK_break)) return 0;
	Token *start = last;
	
	if(!scope->loophost)
		fatal_at(start, "break can only be used inside loops");
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after break");
	
	return new_stmt(BREAK, start, scope);
}

static Stmt *p_continue()
{
	if(!eat(TK_continue)) return 0;
	Token *start = last;
	
	if(!scope->loophost)
		fatal_at(start, "continue can only be used inside loops");
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after continue");
	
	return new_stmt(CONTINUE, start, scope);
}

static Stmt *p_for()
{
	if(!eat(TK_for)) return 0;
	Token *start = last;
	
	Token *iter_name = eat(TK_IDENT);
	if(!iter_name)
		error_after(last, "expected identifier as iterator");
	
	Expr *array = 0;
	Expr *from = 0;
	Expr *to = 0;
	
	if(eat(TK_in)) {
		array = p_expr();
		
		if(!array)
			fatal_after(last, "expected iterable");
	}
	else if(eat(TK_ASSIGN)) {
		from = p_expr();
		if(!from) fatal_at(last, "expected start value after =");
		
		if(!eat(TK_DOTDOT))
			fatal_after(last, "expected .. after start expression");
		
		to = p_expr();
		if(!to) fatal_at(last, "expected end value after ..");
	}
	else {
		error_at(cur, "expected 'in' or '=' after iterator");
	}
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after for-in-head");
	
	enter();
	Decl *iter = new_var(iter_name, scope, iter_name->id, 0, 0, 0, 0);
	declare(iter);
	Scope *blockscope = leave();
	
	if(array) {
		ForEach *foreach = new_foreach(start, blockscope, array, iter, 0);
		blockscope->loophost = (Stmt*)foreach;
		foreach->body = p_block(blockscope);
	
		if(!eat(TK_RCURLY))
			fatal_after(last, "expected } after for-body");
		
		return (Stmt*)foreach;
	}
	else {
		For *forstmt = new_for(start, blockscope, iter, from, to, 0);
		blockscope->loophost = (Stmt*)forstmt;
		forstmt->body = p_block(blockscope);
	
		if(!eat(TK_RCURLY))
			fatal_after(last, "expected } after for-body");
		
		return (Stmt*)forstmt;
	}
}

static Stmt *p_delete()
{
	if(!eat(TK_delete)) return 0;
	Token *start = last;
	Expr *expr = p_expr();
	
	if(!expr)
		fatal_after(last, "expected object to delete");
	
	if(expr->type->kind != PTR)
		fatal_at(expr->start, "expression to delete is not a pointer");
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after delete");
	
	return (Stmt*)new_delete(start, scope, expr);
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl(0, 0)) ||
	(stmt = p_funcdecl(0)) ||
	(stmt = p_structdecl(0)) ||
	(stmt = p_enumdecl(0)) ||
	(stmt = p_uniondecl(0)) ||
	(stmt = p_ifstmt()) ||
	(stmt = p_whilestmt()) ||
	(stmt = p_returnstmt()) ||
	(stmt = p_import()) ||
	(stmt = p_foreign()) ||
	(stmt = p_export()) ||
	(stmt = p_break()) ||
	(stmt = p_continue()) ||
	(stmt = p_for()) ||
	(stmt = p_delete()) ||
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
