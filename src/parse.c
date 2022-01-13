#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "parse.h"

#define match(t) (cur->type == (t))
#define eat(t) (match(t) ? (line = cur[1].line, last = cur++) : 0)

#define expected(w) do { \
	error("expected %s", (w)); \
	exit(EXIT_FAILURE); \
} while(0)

#define expected_after(w, a) do { \
	error("expected %s after %s", (w), (a)); \
	exit(EXIT_FAILURE); \
} while(0)

static Token *cur;
static Token *last;
static int64_t line;

static void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "%" PRId64 ": error: ", line);
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(EXIT_FAILURE);
}

static Token *expect(TokenType type)
{
	Token *token = eat(type);
	if(!token) expected(get_token_type_name(type));
	return token;
}

static Token *expect_after(TokenType type)
{
	Token *token = eat(type);
	if(!token) {
		line = last->line;
		expected_after(
			get_token_type_name(type),
			get_token_type_name(last->type)
		);
	}
	return token;
}

static Expr *new_expr(ExprType type)
{
	Expr *expr = malloc(sizeof(Expr));
	expr->type = type;
	expr->start = last;
	return expr;
}

static Expr *p_atom()
{
	Expr *expr = 0;
	
	if(eat(TK_INT)) {
		expr = new_expr(EX_INT);
		expr->ival = last->ival;
	}
	else if(eat(TK_IDENT)) {
		expr = new_expr(EX_VAR);
		expr->id = last->id;
	}
	
	return expr;
}

static Expr *p_expr()
{
	return p_atom();
}

static Stmt *new_stmt(StmtType type)
{
	Stmt *stmt = malloc(sizeof(Stmt));
	stmt->type = type;
	stmt->start = last;
	return stmt;
}

static Stmt *p_print()
{
	Token *start = eat(TK_print);
	if(!start) return 0;
	Expr *first_expr = 0;
	Expr *last_expr = 0;
	while(1) {
		Expr *expr = p_expr();
		if(!expr) break;
		if(first_expr) last_expr = last_expr->next = expr;
		else first_expr = last_expr = expr;
		expr->next = 0;
		if(!eat(TK_COMMA)) break;
	}
	expect_after(TK_SEMICOLON);
	Stmt *stmt = new_stmt(ST_PRINT);
	stmt->start = start;
	stmt->exprs = first_expr;
	return stmt;
}

static Stmt *p_vardecl()
{
	Token *start = eat(TK_var);
	if(!start) return 0;
	Token *id = expect(TK_IDENT)->id;
	Expr *expr = 0;
	if(eat(TK_ASSIGN)) {
		expr = p_expr();
		if(!expr) expected_after("expression", TK_ASSIGN);
	}
	expect(TK_SEMICOLON);
	Stmt *stmt = new_stmt(ST_DECL);
	stmt->start = start;
	stmt->id = id;
	stmt->expr = expr;
	return stmt;
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl()) ;
	return stmt;
}

static Stmt *p_stmts()
{
	Stmt *first_stmt = 0;
	Stmt *last_stmt = 0;
	while(1) {
		Stmt *stmt = p_stmt();
		if(!stmt) break;
		if(first_stmt) last_stmt = last_stmt->next = stmt;
		else first_stmt = last_stmt = stmt;
		stmt->next = 0;
	}
	return first_stmt;
}

Unit *parse(Token *tokens)
{
	cur = tokens;
	last = 0;
	line = 1;
	Stmt *stmts = p_stmts();
	Unit *unit = malloc(sizeof(Unit));
	unit->stmts = stmts;
	return unit;
}
