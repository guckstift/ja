#ifndef parse_stmt_H
#define parse_stmt_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define parse_stmt_C
#endif

#include "parse_type.c"
#include "parse_expr.c"
#include "ast.c"

Stmt *parse_stmt();
Stmt **parse_stmts();
Block *parse_block(Scope *scope);

#endif
#ifdef parse_stmt_C

#define IMPLEMENT_FLAG

#include <stdio.h>
#include "parse_expr.c"
#include "parse_type.c"
#include "parse_impl.c"
#include "error.c"
#include "array.c"
#include "arena.c"

static Decl *p_vardecl_core(Token *start)
{
	Token *ident = eat(TK_IDENT);
	if(!ident) return 0;
	if(!start) start = ident;
	Type *type = 0;
	Expr *init = 0;
	if(eat(PT_COLON)) type = expect(K_TYPE, "expected a type after ':'");
	if(eat(PT_ASSIGN)) init = expect(K_EXPR, "expected an initializer expression after '='");
	if(type == 0 && init == 0) error_at(start, "variable declaration has neither a type nor an initializer");
	Decl *decl = create_vardecl(ident ? ident->uid : 0, init, type, .stmt.start = start, .stmt.end = peek());
	if(decl->uid) scope_add_decl(decl);
	return decl;
}

static Decl *p_vardecl()
{
	if(!eat(KW_var)) return 0;
	Token *start = peek();
	Decl *decl = p_vardecl_core(start);
	if(!decl) error_at_cur("expected identifier after keyword var");
	expect(PT_SEMICOLON, 0);
	return decl;
}

static Stmt *p_print()
{
	Token *start = peek();
	if(!eat(KW_print)) return 0;
	Expr *value = expect(K_EXPR, 0);
	expect(PT_SEMICOLON, 0);
	return create_print(value, .start = start, .end = peek());
}

static Stmt *p_if()
{
	Token *start = peek();
	if(!eat(KW_if)) return 0;
	Expr *cond = expect(K_EXPR, "expected if-condition");
	expect(PT_LCURLY, 0);
	Block *body = parse_block(0);
	expect(PT_RCURLY, 0);
	Block *else_body = 0;

	if(eat(KW_else)) {
		if(match(KW_if)) {
			enter();
			Stmt *else_if = p_if();
			Stmt **else_stmts = 0;
			if(else_if) array_push(else_stmts, else_if);
			Scope *else_scope = leave();
			else_body = create_block(else_stmts, else_scope);
		}
		else {
			expect(PT_LCURLY, 0);
			else_body = parse_block(0);
			expect(PT_RCURLY, 0);
		}
	}

	return create_if(cond, body, else_body, .start = start, .end = peek());
}

static Stmt *p_while()
{
	Token *start = peek();
	if(!eat(KW_while)) return 0;
	Expr *cond = expect(K_EXPR, "expected while-condition");
	expect(PT_LCURLY, 0);
	Block *body = parse_block(0);
	expect(PT_RCURLY, 0);
	return create_while(cond, body, .start = start, .end = peek());
}

static Stmt *p_import()
{
	Token *start = peek();
	if(!eat(KW_import)) return 0;
	Token *filename = expect(TK_STRING, "expected a module file name to import");
	expect(PT_SEMICOLON, 0);
	return create_import(filename->sval, .start = start, .end = peek());
}

static Decl **p_params()
{
	Decl *first = 0;
	Decl *last = 0;
	int64_t count = 0;

	while(1) {
		Decl *param = p_vardecl_core(0);
		if(!param) break;
		if(first) last = last->stmt.next = param;
		else first = last = param;
		count ++;
		if(!eat(PT_COMMA)) break;
	}

	Decl **params = 0;
	for(Decl *s = first; s; s = s->stmt.next) array_push(params, s);
	return params;
}

static Decl *p_funchead()
{
	Token *start = peek();
	if(!eat(KW_function)) return 0;
	if(getscope()->parent) error_at(start, "functions can only be declared at top level");
	Token *ident = expect(TK_IDENT, "expected a function name");
	Token *uid = ident ? ident->uid : 0;
	Decl **params = 0;
	enter();

	if(expect(PT_LPAREN, "expected parameter list in parenthesis")) {
		params = p_params();
		expect(PT_RPAREN, "expected ) after parameter list");
	}

	Scope *funcscope = leave();
	Type *returntype = create_type(TY_VOID);
	if(eat(PT_COLON)) returntype = expect(K_TYPE, "expected return type after :");
	Decl *decl = create_funcdecl(uid, returntype, params, funcscope, .stmt.start = start);
	if(decl->uid) scope_add_decl(decl);
	return decl;
}

static Decl *p_funcdecl()
{
	Decl *decl = p_funchead();
	if(!decl) return 0;

	if(expect(PT_LCURLY, "expected a function body in curly braces")) {
		decl->body = parse_block(decl->funcscope);
		expect(PT_RCURLY, "expected } after function body");
	}

	decl->stmt.end = peek();
	return decl;
}

static Stmt *p_return()
{
	Token *start = peek();
	if(!eat(KW_return)) return 0;
	if(!getscope()->parent) error_at(start, "return outside of any function");
	Expr *value = parse_expr();
	expect(PT_SEMICOLON, 0);
	return create_return(value, .start = start, .end = peek());
}

static Stmt *p_assign_or_call()
{
	Token *start = peek();
	Expr *target = parse_expr();
	if(!target) return 0;

	if(target->kind == EX_CALL && !match(PT_ASSIGN)) {
		expect(PT_SEMICOLON, 0);
		return create_call(target, .start = start, .end = peek());
	}

	if(!eat(PT_ASSIGN)) {
		setcur(start);
		return 0;
	}

	Expr *value = expect(K_EXPR, "expected expression after '='");
	expect(PT_SEMICOLON, 0);
	return create_assign(target, value, .start = start, .end = peek());
}

Stmt *parse_stmt()
{
	switch(peek()->kind) {
		case KW_function: return (Stmt*)p_funcdecl();
		case KW_if: return p_if();
		case KW_print: return p_print();
		case KW_return: return p_return();
		case KW_var: return (Stmt*)p_vardecl();
		case KW_while: return p_while();
		case KW_import: return p_import();
		default: return p_assign_or_call();
	}
}

Stmt **parse_stmts()
{
	Stmt *first = 0;
	Stmt *last = 0;
	Stmt *stmt = 0;
	int64_t count = 0;
	int invalid_error_raised_before = 0;

	while(1) {
		while(stmt = parse_stmt()) {
			if(first) last = last->next = stmt;
			else first = last = stmt;
			count ++;
			invalid_error_raised_before = 0;
		}

		if(match(PT_RCURLY) || match(TK_EOF))
			break;

		if(!invalid_error_raised_before) {
			error_at_cur("can not find a valid statement");
			invalid_error_raised_before = 1;
		}

		advance();
	}

	Stmt **stmts = 0;
	for(Stmt *s = first; s; s = s->next) array_push(stmts, s);
	return stmts;
}

Block *parse_block(Scope *scope)
{
	if(scope)
		setscope(scope);
	else
		enter();

	Stmt **stmts = parse_stmts();
	Scope *blockscope = leave();
	return create_block(stmts, blockscope);
}

#endif