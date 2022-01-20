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
static Expr *p_expr();

static void error_at(Token *token, char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(token->line, token->linep, src_end, token->start, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void error_at_last(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(last->line, last->linep, src_end, last->start, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void error_at_cur(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(cur->line, cur->linep, src_end, cur->start, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void error_after_last(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(
		last->line, last->linep, src_end, last->start + last->length, msg, args
	);
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

static TypeDesc *new_ptr_type(TypeDesc *subtype)
{
	TypeDesc *dtype = malloc(sizeof(TypeDesc));
	dtype->type = TY_PTR;
	dtype->subtype = subtype;
	return dtype;
}

static int type_equ(TypeDesc *dtype1, TypeDesc *dtype2)
{
	if(dtype1->type == TY_PTR && dtype1->type == dtype2->type) {
		return type_equ(dtype1->subtype, dtype2->subtype);
	}
	
	return dtype1->type == dtype2->type;
}

static TypeDesc *p_type()
{
	if(eat(TK_int)) {
		return new_type(TY_INT64);
	}
	else if(eat(TK_uint)) {
		return new_type(TY_UINT64);
	}
	else if(eat(TK_bool)) {
		return new_type(TY_BOOL);
	}
	else if(eat(TK_GREATER)) {
		TypeDesc *subtype = p_type();
		if(!subtype)
			error_at_last("expected target type");
		return new_ptr_type(subtype);
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
		expr->islvalue = 0;
		expr->dtype = new_type(TY_INT64);
	}
	else if(eat(TK_false) || eat(TK_true)) {
		expr = new_expr(EX_BOOL);
		expr->bval = last->type == TK_true ? true : false;
		expr->isconst = 1;
		expr->islvalue = 0;
		expr->dtype = new_type(TY_BOOL);
	}
	else if(eat(TK_IDENT)) {
		expr = new_expr(EX_VAR);
		expr->id = last->id;
		Stmt *decl = lookup(expr->id);
		if(!decl)
			error_at_last("variable not declared");
		expr->isconst = 0;
		expr->islvalue = 1;
		expr->dtype = decl->dtype;
	}
	
	return expr;
}

static Expr *p_prefix()
{
	if(eat(TK_GREATER)) {
		Expr *expr = new_expr(EX_PTR);
		expr->subexpr = p_prefix();
		if(!expr->subexpr)
			error_after_last("expected target to point to");
		if(!expr->subexpr->islvalue)
			error_at(expr->subexpr->start, "expected target to point to");
		expr->isconst = 0;
		expr->islvalue = 0;
		expr->dtype = new_ptr_type(expr->subexpr->dtype);
		return expr;
	}
	else if(eat(TK_LOWER)) {
		Expr *expr = new_expr(EX_DEREF);
		expr->subexpr = p_prefix();
		if(!expr->subexpr)
			error_at_last("expected expression after <");
		if(expr->subexpr->dtype->type != TY_PTR)
			error_at(expr->subexpr->start, "expected pointer to dereference");
		expr->isconst = 0;
		expr->islvalue = 1;
		expr->dtype = expr->subexpr->dtype->subtype;
		return expr;
	}
	
	return p_atom();
}

static Expr *cast_expr(Expr *subexpr, TypeDesc *dtype)
{
	if(type_equ(subexpr->dtype, dtype)) return subexpr;
	Expr *expr = new_expr(EX_CAST);
	expr->start = subexpr->start;
	expr->isconst = subexpr->isconst;
	expr->islvalue = 0;
	expr->subexpr = subexpr;
	expr->dtype = dtype;
	return expr;
}

static Expr *p_cast()
{
	Expr *expr = p_prefix();
	if(!expr) return 0;
	while(1) {
		if(!eat(TK_as)) break;
		TypeDesc *dtype = p_type();
		if(!dtype)
			error_after_last("expected type after as");
		expr = cast_expr(expr, dtype);
	}
	return expr;
}

static int is_integral_type(TypeDesc *dtype)
{
	Type type = dtype->type;
	return type == TY_INT64 || type == TY_UINT64 || type == TY_BOOL;
}

static Token *p_operator()
{
	Token *op = 0;
	(op = eat(TK_PLUS)) ||
	(op = eat(TK_MINUS)) ;
	return op;
}

static Expr *p_binop()
{
	Expr *left = p_cast();
	if(!left) return 0;
	
	while(1) {
		Token *operator = p_operator();
		if(!operator) break;
		Expr *right = p_cast();
		if(!right)
			error_after_last("expected right side after +");
		Expr *expr = new_expr(EX_BINOP);
		expr->start = left->start;
		expr->left = left;
		expr->right = right;
		expr->operator = operator;
		expr->isconst = left->isconst && right->isconst;
		expr->islvalue = 0;
		TypeDesc *ltype = left->dtype;
		TypeDesc *rtype = right->dtype;
		
		if(is_integral_type(ltype) && is_integral_type(rtype)) {
			expr->dtype = new_type(TY_INT64);
			expr->left = cast_expr(expr->left, expr->dtype);
			expr->right = cast_expr(expr->right, expr->dtype);
		}
		else {
			error_at(
				left->start,
				"left side or right side has incompatible type with operator"
			);
		}
		
		left = expr;
	}
	
	return left;
}

static Expr *p_expr()
{
	return p_binop();
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
		else
			stmt->expr = cast_expr(stmt->expr, stmt->dtype);
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

static Stmt *p_whilestmt()
{
	if(!eat(TK_while)) return 0;
	Stmt *stmt = new_stmt(ST_WHILESTMT);
	stmt->expr = p_expr();
	if(!stmt->expr)
		error_at_last("expected condition after while");
	if(!eat(TK_LCURLY))
		error_after_last("expected { after condition");
	stmt->body = p_stmts();
	if(!eat(TK_RCURLY))
		error_after_last("expected } after while-body");
	return stmt;
}

static Stmt *p_assign()
{
	Expr *target = p_expr();
	if(!target) return 0;
	if(!target->islvalue)
		error_at(target->start, "left side is not assignable");
	Stmt *stmt = new_stmt(ST_ASSIGN);
	stmt->start = target->start;
	stmt->target = target;
	if(!eat(TK_ASSIGN))
		error_after_last("expected = after left side");
	stmt->expr = p_expr();
	if(!stmt->expr)
		error_at_last("expected right side after =");
	stmt->expr = cast_expr(stmt->expr, stmt->target->dtype);
	if(!eat(TK_SEMICOLON))
		error_after_last("expected semicolon after variable declaration");
	return stmt;
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl()) ||
	(stmt = p_ifstmt()) ||
	(stmt = p_whilestmt()) ||
	(stmt = p_assign()) ;
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
	if(!eat(TK_EOF))
		error_at_cur("invalid statement");
	Unit *unit = malloc(sizeof(Unit));
	unit->stmts = stmts;
	return unit;
}
