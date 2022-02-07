#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "parse.h"
#include "print.h"
#include "eval.h"

#define match(t) (cur->type == (t))
#define match2(t1, t2) (cur[0].type == (t1) && cur[1].type == (t2))
#define adv() (last = cur++)
#define eat(t) (match(t) ? adv() : 0)
#define eat2(t1, t2) (match2(t1, t2) ? (adv(), adv()) : 0)

#define error(line, linep, start, ...) \
	print_error(line, linep, src_end, start, __VA_ARGS__)

#define error_at(token, ...) \
	error((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define error_after(token, ...) \
	error( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

#define fatal(line, linep, start, ...) do { \
	error(line, linep, start, __VA_ARGS__); \
	exit(EXIT_FAILURE); \
} while(0)

#define fatal_at(token, ...) \
	fatal((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define fatal_after(token, ...) \
	fatal( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

static Token *cur;
static Token *last;
static char *src_end;
static Scope *scope;

static Stmt *p_stmts(Stmt *func);
static Expr *p_expr();

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

static void enter()
{
	Scope *new_scope = malloc(sizeof(Scope));
	new_scope->parent = scope;
	scope = new_scope;
	scope->first_decl = 0;
	scope->last_decl = 0;
	scope->func = scope->parent ? scope->parent->func : 0;
	scope->struc = 0;
}

static void leave()
{
	scope = scope->parent;
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
	else if(eat(TK_IDENT)) {
		TypeDesc *dtype = new_type(TY_INST);
		dtype->id = last->id;
		Stmt *decl = lookup(dtype->id);
		if(!decl)
			fatal_at(last, "name %t not declared", dtype->id);
		if(decl->type != ST_STRUCTDECL)
			fatal_at(last, "%t is not a structure", dtype->id);
		dtype->typedecl = decl;
		return dtype;
	}
	else if(eat(TK_GREATER)) {
		TypeDesc *subtype = p_type();
		
		if(!subtype)
			fatal_at(last, "expected target type");
		
		return new_ptr_type(subtype);
	}
	else if(eat(TK_LBRACK)) {
		Token *length = eat(TK_INT);
		if(!length)
			fatal_at(cur, "expected integer literal for array length");
		if(length->ival <= 0)
			fatal_at(length, "array length must be greater than 0");
		
		if(!eat(TK_RBRACK))
			fatal_after(last, "expected ] after array length");
		
		TypeDesc *subtype = p_type();
		if(!subtype)
			fatal_at(last, "expected element type");
		
		return new_array_type(length->ival, subtype);
	}
	
	return 0;
}

/*
	Might modify expr and dtype
*/
static Expr *cast_expr(Expr *expr, TypeDesc *dtype, int explicit)
{
	TypeDesc *stype = expr->dtype;
	
	// can not cast from none type
	if(stype->type == TY_NONE)
		fatal_at(expr->start, "expression has no value");
	
	// types equal => no cast needed
	if(type_equ(stype, dtype))
		return expr;
	
	// integral types are castable into other integral types
	if(is_integral_type(stype) && is_integral_type(dtype))
		return new_cast_expr(expr, dtype);
	
	// one pointer to some other by explicit cast always ok
	if(explicit && stype->type == TY_PTR && dtype->type == TY_PTR) {
		return new_cast_expr(expr, dtype);
	}
	
	// arrays with equal length
	if(
		stype->type == TY_ARRAY && dtype->type == TY_ARRAY &&
		stype->length == dtype->length
	) {
		// array literal => cast each item to subtype
		if(expr->type == EX_ARRAY) {
			for(
				Expr *prev = 0, *item = expr->exprs;
				item;
				prev = item, item = item->next
			) {
				Expr *new_item = cast_expr(item, dtype->subtype, explicit);
				if(prev) prev->next = new_item;
				else expr->exprs = new_item;
				new_item->next = item->next;
				item = new_item;
			}
			stype->subtype = dtype->subtype;
			return expr;
		}
		// no array literal => create new array literal with cast items
		else {
			Expr *first = 0;
			Expr *last = 0;
			for(int64_t i=0; i < stype->length; i++) {
				Expr *index = new_int_expr(i, expr->start);
				Expr *subscript = new_subscript(expr, index);
				Expr *item = cast_expr(subscript, dtype->subtype, explicit);
				if(first) {
					last->next = item;
					last = item;
				}
				else {
					first = item;
					last = item;
				}
			}
			expr = new_expr(EX_ARRAY, expr->start);
			expr->exprs = first;
			expr->length = stype->length;
			expr->isconst = 0;
			expr->islvalue = 0;
			expr->dtype = new_type(TY_ARRAY);
			expr->dtype->subtype = dtype->subtype;
			expr->dtype->length = stype->length;
			return expr;
		}
	}
	
	fatal_at(
		expr->start,
		"can not convert type  %y  to  %y  (%s)",
		stype, dtype, explicit ? "explicit" : "implicit"
	);
}

static Expr *p_atom()
{
	if(eat(TK_INT)) {
		return new_int_expr(last->ival, last);
	}
	else if(eat(TK_false) || eat(TK_true)) {
		return new_bool_expr(last->type == TK_true, last);
	}
	else if(eat(TK_IDENT)) {
		Token *ident = last;
		Stmt *decl = lookup(ident->id);
		
		if(!decl)
			fatal_at(last, "name %t not declared", ident);
		
		if(decl->type == ST_STRUCTDECL)
			fatal_at(last, "%t is the name of a structure", ident);
		
		if(decl->type == ST_FUNCDECL)
			return new_var_expr(
				ident->id,
				new_func_type(decl->dtype),
				ident
			);
		
		return new_var_expr(
			ident->id,
			decl->dtype,
			ident
		);
	}
	else if(eat(TK_LBRACK)) {
		Token *start = last;
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
			fatal_after(last, "expected comma or ]");
		if(length == 0)
			fatal_at(last, "empty array literal is not allowed");
		Expr *expr = new_expr(EX_ARRAY, start);
		expr->exprs = first;
		expr->length = length;
		expr->isconst = isconst;
		expr->islvalue = 0;
		expr->dtype = new_type(TY_ARRAY);
		expr->dtype->subtype = subtype;
		expr->dtype->length = length;
		return expr;
	}
	else if(eat(TK_LPAREN)) {
		Token *start = last;
		Expr *expr = p_expr();
		expr->start = start;
		if(!eat(TK_RPAREN)) fatal_after(last, "expected )");
		return expr;
	}
	
	return 0;
}

static Expr *p_postfix()
{
	Expr *expr = p_atom();
	if(!expr) return 0;
	
	while(1) {
		if(eat(TK_as)) {
			TypeDesc *dtype = p_type();
			if(!dtype)
				fatal_after(last, "expected type after as");
			expr = cast_expr(expr, dtype, 1);
		}
		else if(eat(TK_LPAREN)) {
			if(expr->dtype->type != TY_FUNC)
				fatal_at(expr->start, "not a function you are calling");
			if(!eat(TK_RPAREN))
				fatal_after(last, "expected ) after (");
			Expr *call = new_expr(EX_CALL, expr->start);
			call->callee = expr;
			call->isconst = 0;
			call->islvalue = 0;
			call->dtype = expr->dtype->returntype;
			expr = call;
		}
		else if(eat(TK_LBRACK)) {
			if(expr->dtype->type == TY_ARRAY) {
				Expr *index = p_expr();
				if(!index)
					fatal_after(last, "expected index expression after [");
				
				if(!is_integral_type(index->dtype))
					fatal_at(index->start, "index is not integral");
				
				if(
					expr->dtype->type == TY_ARRAY &&
					expr->dtype->length >= 0 &&
					index->isconst
				) {
					int64_t index_val = index->ival;
					if(index_val >= expr->dtype->length)
						fatal_at(
							index->start,
							"index is out of range, must be between 0 .. %u",
							expr->dtype->length - 1
						);
				}
				
				if(!eat(TK_RBRACK))
					fatal_after(last, "expected ] after index expression");
				
				Expr *subscript = new_expr(EX_SUBSCRIPT, expr->start);
				subscript->subexpr = expr;
				subscript->index = index;
				subscript->isconst = 0;
				subscript->islvalue = 1;
				subscript->dtype = expr->dtype->subtype;
				expr = subscript;
			}
			else {
				fatal_after(last, "need array to subscript");
			}
		}
		else if(eat(TK_PERIOD)) {
			TypeDesc *dtype = expr->dtype;
			if(dtype->type != TY_INST)
				fatal_at(expr->start, "no instance to get member");
			Stmt *struct_decl = dtype->typedecl;
			Scope *struct_scope = struct_decl->struct_body->scope;
			Token *id = eat(TK_IDENT);
			if(!id)
				fatal_at(last, "expected id of structure member");
			Expr *member = new_expr(EX_MEMBER, expr->start);
			member->member_id = id->id;
			Stmt *decl = lookup_flat_in(member->member_id, struct_scope);
			if(!decl)
				fatal_at(id, "name %t not declared in struct", id);
			member->isconst = 0;
			member->islvalue = 1;
			member->dtype = decl->dtype;
			member->subexpr = expr;
			expr = member;
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
			fatal_after(last, "expected target to point to");
		if(!expr->subexpr->islvalue)
			fatal_at(expr->subexpr->start, "expected target to point to");
		expr->isconst = 0;
		expr->islvalue = 0;
		expr->dtype = new_ptr_type(expr->subexpr->dtype);
		return expr;
	}
	else if(eat(TK_LOWER)) {
		Expr *subexpr = p_prefix();
		if(!subexpr)
			fatal_at(last, "expected expression after <");
		
		if(subexpr->dtype->type == TY_PTR) {
			Expr *expr = new_expr(EX_DEREF, subexpr->start);
			expr->isconst = 0;
			expr->islvalue = 1;
			expr->dtype = subexpr->dtype->subtype;
			return expr;
		}
		
		fatal_at(subexpr->start, "expected pointer to dereference");
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
			fatal_after(last, "expected right side after %t", operator);
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
			fatal_at(
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
		fatal_after(last, "expected expression to print");
	
	if(
		!is_integral_type(stmt->expr->dtype) &&
		stmt->expr->dtype->type != TY_PTR
	) {
		fatal_at(stmt->expr->start, "can only print numbers or pointers");
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after print statement");
	
	return stmt;
}

static Stmt *p_vardecl()
{
	Token *start = eat(TK_var);
	if(!start) return 0;
	
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_after(last, "expected identifier after keyword var");
	
	TypeDesc *dtype = 0;
	if(eat(TK_COLON)) {
		dtype = p_type();
		if(!dtype) fatal_after(last, "expected type after colon");
	}
	
	Expr *init = 0;
	if(eat(TK_ASSIGN)) {
		init = p_expr();
		if(!init) fatal_after(last, "expected initializer after =");
		
		Token *init_start = init->start;
		
		if(init->dtype->type == TY_FUNC)
			fatal_at(init_start, "can not use function as value");
		
		if(init->dtype->type == TY_NONE)
			fatal_at(init_start, "expression has no value");
	
		if(scope->struc && !init->isconst)
			fatal_at(
				init_start,
				"structure members can only be initialized "
				"with constant values"
			);
		
		if(dtype == 0)
			dtype = init->dtype;
		
		init = cast_expr(init, dtype, 0);
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after variable declaration");
	
	if(dtype == 0)
		fatal_at(ident, "variable without type declared");
	
	if(!is_complete_type(dtype))
		fatal_at(ident, "variable with incomplete type  %y  declared", dtype);
	
	Stmt *stmt = new_stmt(ST_VARDECL, start, scope);
	stmt->id = ident->id;
	stmt->dtype = dtype;
	stmt->expr = init;
	stmt->next_decl = 0;
	
	if(!declare(stmt))
		fatal_at(ident, "name %t already declared", ident);
	
	return stmt;
}

static Stmt *p_vardecls(Stmt *struc)
{
	enter();
	if(struc) scope->struc = struc;
	
	Stmt *first_decl = 0;
	Stmt *last_decl = 0;
	while(1) {
		Stmt *decl = p_vardecl();
		if(!decl) break;
		if(first_decl) last_decl = last_decl->next = decl;
		else first_decl = last_decl = decl;
	}
	
	leave();
	return first_decl;
}

static Stmt *p_funcdecl()
{
	Token *start = eat(TK_function);
	if(!start) return 0;
	
	if(scope->parent)
		fatal_at(last, "functions can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_after(last, "expected identifier after keyword function");
	
	if(!eat(TK_LPAREN))
		fatal_after(last, "expected ( after function name");
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after (");
	
	TypeDesc *dtype = new_type(TY_NONE);
	if(eat(TK_COLON)) {
		dtype = p_type();
		if(!dtype) fatal_after(last, "expected return type after colon");
		
		if(!is_complete_type(dtype))
			fatal_at(
				ident, "function with incomplete type  %y  declared", dtype
			);
	}
	
	Stmt *stmt = new_stmt(ST_FUNCDECL, start, scope);
	stmt->id = ident->id;
	stmt->dtype = dtype;
	stmt->next_decl = 0;
	
	if(!declare(stmt))
		fatal_at(ident, "name %t already declared", ident);
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	stmt->func_body = p_stmts(stmt);
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after function body");
	
	return stmt;
}

static Stmt *p_structdecl()
{
	Token *start = eat(TK_struct);
	if(!start) return 0;
	
	if(scope->parent)
		fatal_at(last, "structures can only be declared at top level");
	
	Token *ident = eat(TK_IDENT);
	if(!ident) fatal_after(last, "expected identifier after keyword struct");
	
	Stmt *stmt = new_stmt(ST_STRUCTDECL, start, scope);
	stmt->id = ident->id;
	stmt->next_decl = 0;
	
	if(!declare(stmt))
		fatal_at(ident, "name %t already declared", ident);
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	stmt->struct_body = p_vardecls(stmt);
	if(stmt->struct_body == 0)
		fatal_at(stmt->start, "empty structure");
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after function body");
	
	return stmt;
}

static Stmt *p_ifstmt()
{
	if(!eat(TK_if)) return 0;
	Stmt *stmt = new_stmt(ST_IFSTMT, last, scope);
	stmt->expr = p_expr_evaled();
	if(!stmt->expr)
		fatal_at(last, "expected condition after if");
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after condition");
	stmt->if_body = p_stmts(0);
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after if-body");
		
	if(eat(TK_else)) {
		if(!eat(TK_LCURLY))
			fatal_after(last, "expected { after else");
		stmt->else_body = p_stmts(0);
		if(!eat(TK_RCURLY))
			fatal_after(last, "expected } after else-body");
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
		fatal_at(last, "expected condition after while");
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected { after condition");
	stmt->while_body = p_stmts(0);
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after while-body");
	return stmt;
}

static Stmt *p_returnstmt()
{
	if(!eat(TK_return)) return 0;
	Stmt *func = scope->func;
	
	if(!func)
		fatal_at(last, "return outside of any function");
	
	TypeDesc *dtype = func->dtype;
	Stmt *stmt = new_stmt(ST_RETURN, last, scope);
	stmt->expr = p_expr();
	
	if(dtype->type == TY_NONE) {
		if(stmt->expr)
			fatal_at(stmt->expr->start, "function should not return values");
	}
	else {
		if(!stmt->expr)
			fatal_after(last, "expected expression to return");
		stmt->expr = cast_expr(stmt->expr, dtype, 0);
		stmt->expr = eval_expr(stmt->expr);
	}
	
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after return statement");
	
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
		fatal_at(target->start, "left side is not assignable");
	Stmt *stmt = new_stmt(ST_ASSIGN, target->start, scope);
	stmt->target = target;
	if(!eat(TK_ASSIGN))
		fatal_after(last, "expected = after left side");
	stmt->expr = p_expr();
	if(!stmt->expr)
		fatal_at(last, "expected right side after =");
	stmt->expr = eval_expr(cast_expr(stmt->expr, stmt->target->dtype, 0));
	if(!eat(TK_SEMICOLON))
		error_after(last, "expected semicolon after variable declaration");
	return stmt;
}

static Stmt *p_stmt()
{
	Stmt *stmt = 0;
	(stmt = p_print()) ||
	(stmt = p_vardecl()) ||
	(stmt = p_funcdecl()) ||
	(stmt = p_structdecl()) ||
	(stmt = p_ifstmt()) ||
	(stmt = p_whilestmt()) ||
	(stmt = p_returnstmt()) ||
	(stmt = p_assign()) ;
	return stmt;
}

static Stmt *p_stmts(Stmt *func)
{
	enter();
	if(func) scope->func = func;
	
	Stmt *first_stmt = 0;
	Stmt *last_stmt = 0;
	while(1) {
		Stmt *stmt = p_stmt();
		if(!stmt) break;
		if(first_stmt) last_stmt = last_stmt->next = stmt;
		else first_stmt = last_stmt = stmt;
	}
	
	leave();
	return first_stmt;
}

Stmt *parse(Tokens *tokens)
{
	cur = tokens->first;
	last = 0;
	src_end = tokens->last->start + tokens->last->length;
	scope = 0;
	Stmt *stmts = p_stmts(0);
	if(!eat(TK_EOF))
		fatal_at(cur, "invalid statement");
	return stmts;
}
