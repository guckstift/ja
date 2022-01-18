#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "parse.h"
#include "print.h"

#define match(t) (cur->type == (t))
#define eat(t) (match(t) ? (last = cur++) : 0)

static Token *cur;
static Token *last;
static char *src_end;
static Scope *scope;

static Stmt *p_stmts();

static void error_at(Token *token, char *msg, ...)
{
	int64_t line = token->line;
	char *linep = token->linep;
	char *err_pos = token->start;
	va_list args;
	va_start(args, msg);
	vprint_error(line, linep, src_end, err_pos, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void error_at_last(char *msg, ...)
{
	int64_t line = last->line;
	char *linep = last->linep;
	char *err_pos = last->start;
	va_list args;
	va_start(args, msg);
	vprint_error(line, linep, src_end, err_pos, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void error_after_last(char *msg, ...)
{
	int64_t line = last->line;
	char *linep = last->linep;
	char *err_pos = last->start + last->length;
	va_list args;
	va_start(args, msg);
	vprint_error(line, linep, src_end, err_pos, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static Stmt *lookup_flat_in(Token *id, Scope *scope)
{
	for(Stmt *decl = scope->first_decl; decl; decl = decl->next_decl) {
		if(decl->id == id) return decl;
	}
	
	return 0;
}

static Stmt *lookup_in(Token *id, Scope *scope)
{
	Stmt *decl = lookup_flat_in(id, scope);
	
	if(decl) {
		return decl;
	}
	
	if(scope->parent) {
		return lookup_in(id, scope->parent);
	}
	
	return 0;
}

static Stmt *lookup_flat(Token *id)
{
	return lookup_flat_in(id, scope);
}

static Stmt *lookup(Token *id)
{
	return lookup_in(id, scope);
}

static int declare(Stmt *new_decl)
{
	if(lookup_flat(new_decl->id))
		return 0;
	
	if(scope->first_decl)
		scope->last_decl = scope->last_decl->next_decl = new_decl;
	else
		scope->first_decl = scope->last_decl = new_decl;
	
	return 1;
}

static TypeDesc *new_type(Type type)
{
	TypeDesc *dtype = malloc(sizeof(TypeDesc));
	dtype->type = type;
	return dtype;
}

static int type_equ(TypeDesc *dtype1, TypeDesc *dtype2)
{
	return dtype1->type == dtype2->type;
}

static TypeDesc *p_type()
{
	if(eat(TK_int)) {
		return new_type(TY_INT64);
	}
	else if(eat(TK_bool)) {
		return new_type(TY_BOOL);
	}
	
	return 0;
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
		expr->isconst = 1;
		expr->dtype = new_type(TY_INT64);
	}
	else if(eat(TK_false) || eat(TK_true)) {
		expr = new_expr(EX_BOOL);
		expr->bval = last->type == TK_true ? true : false;
		expr->isconst = 1;
		expr->dtype = new_type(TY_BOOL);
	}
	else if(eat(TK_IDENT)) {
		expr = new_expr(EX_VAR);
		expr->id = last->id;
		Stmt *decl = lookup(expr->id);
		if(!decl)
			error_at_last("variable not declared");
		expr->isconst = 0;
		expr->dtype = decl->dtype;
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
	stmt->next = 0;
	stmt->scope = scope;
	return stmt;
}

static Stmt *p_print()
{
	if(!eat(TK_print)) return 0;
	Stmt *stmt = new_stmt(ST_PRINT);
	stmt->expr = p_expr();
	if(!stmt->expr)
		error_after_last("expected expression to print");
	if(!eat(TK_SEMICOLON))
		error_after_last("expected semicolon after print statement");
	return stmt;
}

static Stmt *p_vardecl()
{
	if(!eat(TK_var)) return 0;
	Stmt *stmt = new_stmt(ST_VARDECL);
	stmt->next_decl = 0;
	Token *id = eat(TK_IDENT);
	if(!id)
		error_after_last("expected identifier after keyword var");
	stmt->id = id->id;
	
	if(eat(TK_COLON)) {
		stmt->dtype = p_type();
		if(!stmt->dtype)
			error_after_last("expected type after colon");
	}
	else {
		stmt->dtype = 0;
	}
	
	if(eat(TK_ASSIGN)) {
		stmt->expr = p_expr();
		if(!stmt->expr)
			error_after_last("expected initializer after equals");
		if(stmt->dtype == 0)
			stmt->dtype = stmt->expr->dtype;
		else if(!type_equ(stmt->expr->dtype, stmt->dtype))
			error_at(stmt->expr->start, "type mismatch");
	}
	else {
		stmt->expr = 0;
	}
	
	if(!declare(stmt))
		error_at(id, "variable already declared");
	if(!eat(TK_SEMICOLON))
		error_after_last("expected semicolon after variable declaration");
	return stmt;
}

static Stmt *p_ifstmt()
{
	if(!eat(TK_if)) return 0;
	Stmt *stmt = new_stmt(ST_IFSTMT);
	stmt->expr = p_expr();
	if(!stmt->expr)
		error_at_last("expected condition after if");
	if(!eat(TK_LCURLY))
		error_after_last("expected { after condition");
	stmt->body = p_stmts();
	if(!eat(TK_RCURLY))
		error_after_last("expected } after if-body");
		
	if(eat(TK_else)) {
		if(!eat(TK_LCURLY))
			error_after_last("expected { after else");
		stmt->else_body = p_stmts();
		if(!eat(TK_RCURLY))
			error_after_last("expected } after else-body");
	}
	else {
		stmt->else_body = 0;
	}
	
	return stmt;
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl()) ||
	(stmt = p_ifstmt()) ;
	return stmt;
}

static Stmt *p_stmts()
{
	Scope *new_scope = malloc(sizeof(Scope));
	new_scope->parent = scope;
	scope = new_scope;
	scope->first_decl = 0;
	scope->last_decl = 0;
	Stmt *first_stmt = 0;
	Stmt *last_stmt = 0;
	while(1) {
		Stmt *stmt = p_stmt();
		if(!stmt) break;
		if(first_stmt) last_stmt = last_stmt->next = stmt;
		else first_stmt = last_stmt = stmt;
	}
	scope = scope->parent;
	return first_stmt;
}

Unit *parse(Tokens *tokens)
{
	cur = tokens->first;
	last = 0;
	src_end = tokens->last->start + tokens->last->length;
	scope = 0;
	Stmt *stmts = p_stmts();
	Unit *unit = malloc(sizeof(Unit));
	unit->stmts = stmts;
	return unit;
}
