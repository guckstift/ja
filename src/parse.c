#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "parse.h"
#include "print.h"
#include "eval.h"

#define match(t) (cur->type == (t))
#define eat(t) (match(t) ? (last = cur++) : 0)

static Token *cur;
static Token *last;
static char *src_end;
static Scope *scope;
static int noexit;

static Stmt *p_stmts(Stmt *func);
static Expr *p_expr();

static void error_at(Token *token, char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(token->line, token->linep, src_end, token->start, msg, args);
	va_end(args);
	if(noexit) {noexit = 0; return;}
	exit(EXIT_FAILURE);
}

static void error_at_last(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(last->line, last->linep, src_end, last->start, msg, args);
	va_end(args);
	if(noexit) {noexit = 0; return;}
	exit(EXIT_FAILURE);
}

static void error_at_cur(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(cur->line, cur->linep, src_end, cur->start, msg, args);
	va_end(args);
	if(noexit) {noexit = 0; return;}
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
	if(noexit) {noexit = 0; return;}
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

static TypeDesc *p_type()
{
	if(eat(TK_int)) {
		return new_type(TY_INT64);
	}
	else if(eat(TK_uint8)) {
		return new_type(TY_UINT8);
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
	else if(eat(TK_LBRACK)) {
		Token *length = eat(TK_INT);
		if(length) {
			if(length->ival <= 0)
				error_at(length, "array length must be greater than 0");
		}
		if(!eat(TK_RBRACK)) {
			if(!length)
				error_at_cur("expected integer literal for array length or ]");
			else
				error_after_last("expected ] after array length");
		}
		TypeDesc *subtype = p_type();
		if(!subtype)
			error_at_last("expected element type");
		return new_array_type(length ? length->ival : -1, subtype);
	}
	
	return 0;
}

static Expr *cast_expr(Expr *src_expr, TypeDesc *dtype, int explicit)
{
	TypeDesc *src_type = src_expr->dtype;
	
	// can not cast from none type
	if(src_type->type == TY_NONE)
		error_at(src_expr->start, "expression has no value");
	
	// types equal => no cast needed
	if(type_equ(src_type, dtype))
		return src_expr;
	
	// integral types are castable into other integral types
	if(is_integral_type(src_type) && is_integral_type(dtype))
		return new_cast_expr(src_expr, dtype);
	
	// one pointer to some other possible only by explicit cast
	if(explicit && src_type->type == TY_PTR && dtype->type == TY_PTR) {
		// automatic array length determination
		/*
		TypeDesc *dt = dtype;
		TypeDesc *et = src_type;
		while(dt->type == TY_ARRAY || dt->type == TY_PTR) {
			if(dt->type == TY_ARRAY && dt->length == -1)
				dt->length = et->length;
			dt = dt->subtype;
			et = et->subtype;
		}
		*/
		return new_cast_expr(src_expr, dtype);
	}
	
	// arrays with equal length or undefined target length
	if(
		src_type->type == TY_ARRAY && dtype->type == TY_ARRAY &&
		(src_type->length == dtype->length || dtype->length == -1)
	) {
		// array literal => cast each item to subtype
		if(src_expr->type == EX_ARRAY) {
			for(
				Expr *prev = 0, *item = src_expr->exprs;
				item;
				prev = item, item = item->next
			) {
				Expr *new_item = cast_expr(item, dtype->subtype, explicit);
				if(prev) prev->next = new_item;
				else src_expr->exprs = new_item;
				new_item->next = item->next;
				item = new_item;
			}
			src_type->subtype = dtype->subtype;
			return src_expr;
		}
		// no array literal => create new array literal with cast items
		else {
			Expr *first = 0;
			Expr *last_expr = 0;
			for(int64_t i=0; i < src_type->length; i++) {
				Expr *index = new_int_expr(i, src_expr->start);
				Expr *subscript = new_subscript(src_expr, index);
				Expr *item = cast_expr(subscript, dtype->subtype, explicit);
				if(first) {
					last_expr->next = item;
					last_expr = item;
				}
				else {
					first = item;
					last_expr = item;
				}
			}
			Expr *expr = new_expr(EX_ARRAY, src_expr->start);
			expr->exprs = first;
			expr->length = src_type->length;
			expr->isconst = 0;
			expr->islvalue = 0;
			expr->dtype = new_type(TY_ARRAY);
			expr->dtype->subtype = dtype->subtype;
			expr->dtype->length = src_type->length;
			return expr;
		}
	}
	
	error_at(
		src_expr->start,
		"can not convert type  %y  to  %y  (%s)",
		src_type, dtype, explicit ? "explicit" : "implicit"
	);
}

static Expr *p_atom()
{
	Expr *expr = 0;
	Token *start;
	
	if(eat(TK_INT)) {
		expr = new_expr(EX_INT, last);
		expr->ival = last->ival;
		expr->isconst = 1;
		expr->islvalue = 0;
		expr->dtype = new_type(TY_INT64);
	}
	else if(eat(TK_false) || eat(TK_true)) {
		expr = new_expr(EX_BOOL, last);
		expr->ival = last->type == TK_true ? 1 : 0;
		expr->isconst = 1;
		expr->islvalue = 0;
		expr->dtype = new_type(TY_BOOL);
	}
	else if(eat(TK_IDENT)) {
		expr = new_expr(EX_VAR, last);
		expr->id = last->id;
		Stmt *decl = lookup(expr->id);
		if(!decl)
			error_at_last("name %t not declared", expr->id);
		if(decl->type == ST_FUNCDECL) {
			expr->isconst = 0;
			expr->islvalue = 0;
			expr->dtype = decl->dtype;
		}
		else {
			expr->isconst = 0;
			expr->islvalue = 1;
			expr->dtype = decl->dtype;
		}
	}
	else if(start = eat(TK_LBRACK)) {
		Expr *first = 0;
		Expr *last_expr = 0;
		TypeDesc *subtype = 0;
		uint64_t length = 0;
		int isconst = 1;
		while(1) {
			Expr *item = p_expr();
			if(!item) break;
			isconst = isconst && item->isconst;
			if(first) {
				if(!type_equ(subtype, item->dtype)) {
					item = cast_expr(item, subtype, 0);
				}
				last_expr->next = item;
				last_expr = item;
			}
			else {
				first = item;
				last_expr = item;
				subtype = item->dtype;
			}
			length ++;
			if(!eat(TK_COMMA)) break;
		}
		if(!eat(TK_RBRACK))
			error_after_last("expected comma or ]");
		if(length == 0)
			error_at_last("empty array literal is not allowed");
		expr = new_expr(EX_ARRAY, start);
		expr->exprs = first;
		expr->length = length;
		expr->isconst = isconst;
		expr->islvalue = 0;
		expr->dtype = new_type(TY_ARRAY);
		expr->dtype->subtype = subtype;
		expr->dtype->length = length;
	}
	
	return expr;
}

static Expr *p_postfix()
{
	Expr *expr = p_atom();
	if(!expr) return 0;
	while(1) {
		if(eat(TK_as)) {
			TypeDesc *dtype = p_type();
			if(!dtype)
				error_after_last("expected type after as");
			expr = cast_expr(expr, dtype, 1);
		}
		else if(eat(TK_LPAREN)) {
			if(expr->dtype->type != TY_FUNC)
				error_at(expr->start, "not a function you are calling");
			if(!eat(TK_RPAREN))
				error_after_last("expected ) after (");
			Expr *call = new_expr(EX_CALL, expr->start);
			call->callee = expr;
			call->isconst = 0;
			call->islvalue = 0;
			call->dtype = expr->dtype->returntype;
			expr = call;
		}
		else if(eat(TK_LBRACK)) {
			if(expr->dtype->type != TY_ARRAY)
				error_after_last("need array to subscript");
			Expr *index = p_expr();
			if(!index)
				error_after_last("expected index expression after [");
			if(!is_integral_type(index->dtype))
				error_at(index->start, "index is not integral");
			index = eval_expr(index);
			if(index->isconst) {
				int64_t index_val = index->ival;
				if(index_val >= expr->dtype->length)
					error_at(
						index->start,
						"index is out of range, must be between 0 .. %u",
						expr->dtype->length - 1
					);
			}
			if(!eat(TK_RBRACK))
				error_after_last("expected ] after index expression");
			Expr *subscript = new_expr(EX_SUBSCRIPT, expr->start);
			subscript->subexpr = expr;
			subscript->index = index;
			subscript->isconst = 0;
			subscript->islvalue = 1;
			subscript->dtype = expr->dtype->subtype;
			expr = subscript;
		}
		else {
			break;
		}
	}
	return expr;
}

static Expr *p_prefix()
{
	if(eat(TK_GREATER)) {
		Expr *expr = new_expr(EX_PTR, last);
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
		Expr *expr = new_expr(EX_DEREF, last);
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
	
	return p_postfix();
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
	Expr *left = p_prefix();
	if(!left) return 0;
	
	while(1) {
		Token *operator = p_operator();
		if(!operator) break;
		Expr *right = p_prefix();
		if(!right)
			error_after_last("expected right side after %t", operator);
		Expr *expr = new_expr(EX_BINOP, left->start);
		expr->left = left;
		expr->right = right;
		expr->operator = operator;
		expr->isconst = left->isconst && right->isconst;
		expr->islvalue = 0;
		TypeDesc *ltype = left->dtype;
		TypeDesc *rtype = right->dtype;
		
		if(is_integral_type(ltype) && is_integral_type(rtype)) {
			expr->dtype = new_type(TY_INT64);
			expr->left = cast_expr(expr->left, expr->dtype, 0);
			expr->right = cast_expr(expr->right, expr->dtype, 0);
		}
		else {
			error_at(
				expr->operator,
				"can not use types  %y  and  %y  with operator %t",
				ltype, rtype, expr->operator
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

static Expr *p_expr_evaled()
{
	return eval_expr(p_expr());
}

static Stmt *p_print()
{
	if(!eat(TK_print)) return 0;
	Stmt *stmt = new_stmt(ST_PRINT, last, scope);
	stmt->expr = p_expr_evaled();
	
	if(!stmt->expr)
		error_after_last("expected expression to print");
	
	if(
		!is_integral_type(stmt->expr->dtype) &&
		stmt->expr->dtype->type != TY_PTR
	) {
		error_at(stmt->expr->start, "can only print numbers or pointers");
	}
	
	if(!eat(TK_SEMICOLON))
		noexit=1,error_after_last("expected semicolon after print statement");
	
	return stmt;
}

static Stmt *p_vardecl()
{
	if(!eat(TK_var)) return 0;
	Stmt *stmt = new_stmt(ST_VARDECL, last, scope);
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
		if(stmt->expr->dtype->type == TY_FUNC)
			error_at_last("can not use functions as values");
		if(stmt->dtype == 0)
			stmt->dtype = stmt->expr->dtype;
		else
			stmt->expr = cast_expr(stmt->expr, stmt->dtype, 0);
		
		stmt->expr = eval_expr(stmt->expr);
		
		if(stmt->expr->dtype->type == TY_NONE)
			error_at(stmt->expr->start, "expression has no value");
		
		TypeDesc *dtype = stmt->dtype;
		
		// automatic array length from init
		TypeDesc *dt = dtype;
		TypeDesc *et = stmt->expr->dtype;
		while(dt->type == TY_ARRAY || dt->type == TY_PTR) {
			if(dt->type == TY_ARRAY && dt->length == -1)
				dt->length = et->length;
			dt = dt->subtype;
			et = et->subtype;
		}
	}
	else {
		stmt->expr = 0;
	}
	
	if(stmt->expr == 0 && stmt->dtype == 0)
		error_at(id, "variable without type declared");
	
	TypeDesc *dt = stmt->dtype;
	while(dt->type == TY_ARRAY) {
		if(dt->length == -1)
			error_at(
				id, "variable with incomplete type declared  %y", stmt->dtype
			);
		dt = dt->subtype;
	}
	
	if(!declare(stmt))
		error_at(id, "name %t already declared", id);
	if(!eat(TK_SEMICOLON))
		noexit=1,
		error_after_last("expected semicolon after variable declaration");
	return stmt;
}

static Stmt *p_funcdecl()
{
	if(!eat(TK_function)) return 0;
	
	if(scope->parent)
		error_at_last("functions can only be declared at top level");
	
	Stmt *stmt = new_stmt(ST_FUNCDECL, last, scope);
	stmt->next_decl = 0;
	Token *id = eat(TK_IDENT);
	if(!id)
		error_after_last("expected identifier after keyword function");
	stmt->id = id->id;
	
	if(!eat(TK_LPAREN))
		error_after_last("expected ( after function name");
	if(!eat(TK_RPAREN))
		error_after_last("expected ) after (");
	
	stmt->dtype = new_type(TY_FUNC);
	
	if(eat(TK_COLON)) {
		Token *start = cur;
		stmt->dtype->returntype = p_type();
	}
	else {
		stmt->dtype->returntype = new_type(TY_NONE);
	}
	
	if(!declare(stmt))
		error_at(id, "name %t already declared", id);
	
	if(!eat(TK_LCURLY))
		error_after_last("expected {");
	stmt->func_body = p_stmts(stmt);
	if(!eat(TK_RCURLY))
		error_after_last("expected } after function body");
	
	return stmt;
}

static Stmt *p_ifstmt()
{
	if(!eat(TK_if)) return 0;
	Stmt *stmt = new_stmt(ST_IFSTMT, last, scope);
	stmt->expr = p_expr_evaled();
	if(!stmt->expr)
		error_at_last("expected condition after if");
	if(!eat(TK_LCURLY))
		error_after_last("expected { after condition");
	stmt->body = p_stmts(0);
	if(!eat(TK_RCURLY))
		error_after_last("expected } after if-body");
		
	if(eat(TK_else)) {
		if(!eat(TK_LCURLY))
			error_after_last("expected { after else");
		stmt->else_body = p_stmts(0);
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
	Stmt *stmt = new_stmt(ST_WHILESTMT, last, scope);
	stmt->expr = p_expr_evaled();
	if(!stmt->expr)
		error_at_last("expected condition after while");
	if(!eat(TK_LCURLY))
		error_after_last("expected { after condition");
	stmt->body = p_stmts(0);
	if(!eat(TK_RCURLY))
		error_after_last("expected } after while-body");
	return stmt;
}

static Stmt *p_returnstmt()
{
	if(!eat(TK_return)) return 0;
	Stmt *func = scope->func;
	
	if(!func)
		error_at_last("return outside of any function");
	
	TypeDesc *returntype = func->dtype->returntype;
	Stmt *stmt = new_stmt(ST_RETURN, last, scope);
	stmt->expr = p_expr();
	
	if(returntype->type == TY_NONE) {
		if(stmt->expr)
			error_at(stmt->expr->start, "function should not return values");
	}
	else {
		if(!stmt->expr)
			error_after_last("expected expression to return");
		stmt->expr = cast_expr(stmt->expr, returntype, 0);
		stmt->expr = eval_expr(stmt->expr);
	}
	
	if(!eat(TK_SEMICOLON))
		noexit=1,error_after_last("expected semicolon after return statement");
	
	return stmt;
}

static Stmt *p_assign()
{
	Expr *target = p_expr_evaled();
	if(!target) return 0;
	
	if(target->type == EX_CALL && eat(TK_SEMICOLON)) {
		Stmt *stmt = new_stmt(ST_CALL, target->start, scope);
		stmt->call = target;
		return stmt;
	}
	
	if(!target->islvalue)
		error_at(target->start, "left side is not assignable");
	Stmt *stmt = new_stmt(ST_ASSIGN, target->start, scope);
	stmt->target = target;
	if(!eat(TK_ASSIGN))
		error_after_last("expected = after left side");
	stmt->expr = p_expr();
	if(!stmt->expr)
		error_at_last("expected right side after =");
	stmt->expr = eval_expr(cast_expr(stmt->expr, stmt->target->dtype, 0));
	if(!eat(TK_SEMICOLON))
		noexit=1,
		error_after_last("expected semicolon after variable declaration");
	return stmt;
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl()) ||
	(stmt = p_funcdecl()) ||
	(stmt = p_ifstmt()) ||
	(stmt = p_whilestmt()) ||
	(stmt = p_returnstmt()) ||
	(stmt = p_assign()) ;
	return stmt;
}

static Stmt *p_stmts(Stmt *func)
{
	Scope *new_scope = malloc(sizeof(Scope));
	new_scope->parent = scope;
	scope = new_scope;
	scope->first_decl = 0;
	scope->last_decl = 0;
	
	if(func)
		scope->func = func;
	else if(scope->parent)
		scope->func = scope->parent->func;
	else
		scope->func = 0;
	
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

Stmt *parse(Tokens *tokens)
{
	noexit = 0;
	cur = tokens->first;
	last = 0;
	src_end = tokens->last->start + tokens->last->length;
	scope = 0;
	Stmt *stmts = p_stmts(0);
	if(!eat(TK_EOF))
		error_at_cur("invalid statement");
	return stmts;
}
