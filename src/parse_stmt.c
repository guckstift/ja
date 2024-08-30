#include <stdio.h>
#include "parse_stmt.h"
#include "parse_expr.h"
#include "parse_type.h"
#include "parse_impl.h"
#include "error.h"
#include "array.h"
#include "arena.h"

static Stmt *p_vardecl()
{
	Token *start = peek();

	if(!eat(KW_var))
		return 0;

	Token *ident = expect(TK_IDENT, 0);
	Type *type = 0;
	Expr *init = 0;

	if(eat(PT_COLON))
		type = expect(K_TYPE, "expected a type after ':'");

	if(eat(PT_ASSIGN))
		init = expect(K_EXPR, "expected an initializer expression after '='");

	expect(PT_SEMICOLON, 0);

	if(type == 0 && init == 0)
		error_at(start, "variable declaration has neither a type nor an initializer");

	Stmt *stmt = create_vardecl(ident ? ident->id : 0, type, init, .start = start, .end = peek());

	if(stmt->id)
		if(declare(stmt) == 0)
			error_at(ident, "name %t already declared", ident);

	return stmt;
}

static Stmt *p_assign()
{
	Token *start = peek();
	Expr *target = parse_expr();

	if(!target)
		return 0;

	if(!eat(PT_ASSIGN)) {
		setcur(start);
		return 0;
	}

	Expr *value = expect(K_EXPR, "expected expression after '='");
	expect(PT_SEMICOLON, 0);
	return create_assign(target, value, .start = start, .end = peek());
}

static Stmt *p_print()
{
	Token *start = peek();

	if(!eat(KW_print))
		return 0;

	Expr *value = expect(K_EXPR, 0);
	expect(PT_SEMICOLON, 0);
	return create_print(value, .start = start, .end = peek());
}

static Stmt *p_if()
{
	Token *start = peek();

	if(!eat(KW_if))
		return 0;

	Expr *cond = expect(K_EXPR, "expected if-condition");
	expect(PT_LCURLY, 0);
	Block *body = parse_block(0);
	expect(PT_RCURLY, 0);
	Block *else_body = 0;

	if(eat(KW_else)) {
		expect(PT_LCURLY, 0);
		else_body = parse_block(0);
		expect(PT_RCURLY, 0);
	}

	return create_if(cond, body, else_body, .start = start, .end = peek());
}

static Stmt *p_while()
{
	Token *start = peek();

	if(!eat(KW_while))
		return 0;

	Expr *cond = expect(K_EXPR, "expected while-condition");
	expect(PT_LCURLY, 0);
	Block *body = parse_block(0);
	expect(PT_RCURLY, 0);
	return create_while(cond, body, .start = start, .end = peek());
}

static Stmt *p_funcdecl()
{
	Token *start = peek();

	if(!eat(KW_function))
		return 0;

	Token *ident = expect(TK_IDENT, 0);
	expect(PT_LPAREN, 0);
	expect(PT_RPAREN, 0);
	expect(PT_LCURLY, 0);
	Block *body = parse_block(0);
	expect(PT_RCURLY, 0);
	Stmt *stmt = create_funcdecl(ident ? ident->id : 0, body, .start = start, .end = peek());

	if(stmt->id)
		if(declare(stmt) == 0)
			error_at(ident, "name %t already declared", ident);

	return stmt;
}

Stmt *parse_stmt()
{
	Stmt *stmt;
	(stmt = p_vardecl()) ||
	(stmt = p_print()) ||
	(stmt = p_if()) ||
	(stmt = p_while()) ||
	(stmt = p_funcdecl()) ||
	(stmt = p_assign()) ;
	return stmt;
}

Stmt **parse_stmts()
{
	Stmt **stmts = 0;
	Stmt *stmt = 0;
	int invalid_error_raised_before = 0;

	while(1) {
		while(stmt = parse_stmt()) {
			array_push(stmts, stmt);
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
	Block *block = alloc(sizeof(Block));
	block->stmts = stmts;
	block->scope = blockscope;
	return block;
}