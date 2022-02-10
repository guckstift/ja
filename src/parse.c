#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "parse.h"
#include "print.h"
#include "eval.h"
#include "build.h"
#include "utils.h"

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
static char *unit_id;

static Stmt *p_stmts(Decl *func);
static Expr *p_expr();
static Type *p_type();
static Expr *p_prefix();

static Decl *lookup_builtin(Token *id)
{
	if(scope->parent) return 0;
	
	if(token_text_equals(id, "argv")) {
		static Decl *argv = 0;
		if(argv == 0) {
			argv = new_vardecl(
				id, new_ptr_type(new_array_type(-1, new_type(STRING))),
				0, 0, scope
			);
		}
		return argv;
	}
	
	return 0;
}

static Decl *lookup_flat(Token *id)
{
	Decl *decl = lookup_builtin(id);
	if(decl) return decl;
	return lookup_flat_in(id, scope);
}

static Decl *lookup(Token *id)
{
	Decl *decl = lookup_builtin(id);
	if(decl) return decl;
	return lookup_in(id, scope);
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

/*
	Might modify expr and dtype
*/
static Expr *cast_expr(Expr *expr, Type *dtype, int explicit)
{
	Type *stype = expr->dtype;
	
	// can not cast from none type
	if(stype->kind == NONE)
		fatal_at(expr->start, "expression has no value");
	
	// types equal => no cast needed
	if(type_equ(stype, dtype))
		return expr;
	
	// integral types are castable into other integral types
	if(is_integral_type(stype) && is_integral_type(dtype))
		return eval_integral_cast(expr, dtype);
	
	// one pointer to some other by explicit cast always ok
	if(explicit && stype->kind == PTR && dtype->kind == PTR)
		return new_cast_expr(expr, dtype);
	
	// array ptr to dynamic array ptr when same item type
	if(
		stype->kind == PTR && stype->subtype->kind == ARRAY &&
		is_dynarray_ptr_type(dtype) &&
		type_equ(stype->subtype->itemtype, dtype->subtype->itemtype)
	) {
		return new_cast_expr(expr, dtype);
	}
	
	// arrays with equal length
	if(
		stype->kind == ARRAY && dtype->kind == ARRAY &&
		stype->length == dtype->length
	) {
		// array literal => cast each item to itemtype
		if(expr->kind == ARRAY) {
			for(
				Expr *prev = 0, *item = expr->exprs;
				item;
				prev = item, item = item->next
			) {
				Expr *new_item = cast_expr(item, dtype->itemtype, explicit);
				if(prev) prev->next = new_item;
				else expr->exprs = new_item;
				new_item->next = item->next;
				item = new_item;
			}
			stype->itemtype = dtype->itemtype;
			return expr;
		}
		// no array literal => create new array literal with cast items
		else {
			Expr *first = 0;
			Expr *last = 0;
			for(int64_t i=0; i < stype->length; i++) {
				Expr *index = new_int_expr(i, expr->start);
				Expr *subscript = new_subscript(expr, index);
				Expr *item = cast_expr(subscript, dtype->itemtype, explicit);
				headless_list_push(first, last, next, item);
			}
			return new_array_expr(
				first, stype->length, expr->isconst,
				dtype->itemtype, expr->start
			);
		}
	}
	
	fatal_at(
		expr->start,
		"can not convert type  %y  to  %y  (%s)",
		stype, dtype, explicit ? "explicit" : "implicit"
	);
}

static Expr *p_var()
{
	if(!eat(TK_IDENT)) return 0;
	Token *ident = last;
	Decl *decl = lookup(ident->id);
	
	if(!decl)
		fatal_at(last, "name %t not declared", ident);
	
	if(decl->kind == STRUCT)
		fatal_at(last, "%t is the name of a structure", ident);
	
	if(decl->kind == FUNC)
		return new_var_expr(ident->id, new_func_type(decl->dtype), ident);
	
	return new_var_expr(ident->id, decl->dtype, ident);
}

static Expr *p_array()
{
	if(!eat(TK_LBRACK)) return 0;
	
	Token *start = last;
	Expr *first = 0;
	Expr *last_expr = 0;
	Type *itemtype = 0;
	uint64_t length = 0;
	int isconst = 1;
	
	while(1) {
		Expr *item = p_expr();
		if(!item) break;
		isconst = isconst && item->isconst;
		if(first) {
			if(!type_equ(itemtype, item->dtype)) {
				item = cast_expr(item, itemtype, 0);
			}
		}
		else {
			itemtype = item->dtype;
		}
		headless_list_push(first, last_expr, next, item);
		length ++;
		if(!eat(TK_COMMA)) break;
	}
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected comma or ]");
	
	if(length == 0)
		fatal_at(last, "empty array literal is not allowed");
	
	return new_array_expr(first, length, isconst, itemtype, start);
}

static Expr *p_atom()
{
	if(eat(TK_INT))
		return new_int_expr(last->ival, last);
	
	if(eat(TK_false) || eat(TK_true))
		return new_bool_expr(last->type == TK_true, last);
	
	if(eat(TK_STRING))
		return new_string_expr(last->string, last->string_length, last);
		
	if(eat(TK_LPAREN)) {
		Token *start = last;
		Expr *expr = p_expr();
		expr->start = start;
		if(!eat(TK_RPAREN)) fatal_after(last, "expected )");
		return expr;
	}
	
	Expr *expr = 0;
	(expr = p_var()) ||
	(expr = p_array()) ;
	return expr;
}

static Expr *p_cast_x(Expr *expr)
{
	if(!eat(TK_as)) return 0;
	
	Type *dtype = p_type();
	if(!dtype)
		fatal_after(last, "expected type after as");
	
	return cast_expr(expr, dtype, 1);
}

static Expr *p_call_x(Expr *expr)
{
	if(!eat(TK_LPAREN)) return 0;
	
	if(expr->dtype->kind != FUNC)
		fatal_at(expr->start, "not a function you are calling");
	
	if(!eat(TK_RPAREN))
		fatal_after(last, "expected ) after (");
	
	Expr *call = new_expr(CALL, expr->start);
	call->callee = expr;
	call->isconst = 0;
	call->islvalue = 0;
	call->dtype = expr->dtype->returntype;
	return call;
}

static Expr *p_subscript_x(Expr *expr)
{
	if(!eat(TK_LBRACK)) return 0;
	
	while(expr->dtype->kind == PTR) {
		expr = new_deref_expr(expr);
	}
	
	if(expr->dtype->kind != ARRAY) {
		fatal_after(last, "need array to subscript");
	}
	
	Expr *index = p_expr();
	if(!index)
		fatal_after(last, "expected index expression after [");
	
	if(!is_integral_type(index->dtype))
		fatal_at(index->start, "index is not integral");
	
	if(
		expr->dtype->kind == ARRAY &&
		expr->dtype->length >= 0 &&
		index->isconst
	) {
		if(index->ival < 0 || index->ival >= expr->dtype->length)
			fatal_at(
				index->start,
				"index is out of range, must be between 0 .. %u",
				expr->dtype->length - 1
			);
	}
	
	if(!eat(TK_RBRACK))
		fatal_after(last, "expected ] after index expression");
	
	return eval_subscript(expr, index);
}

static Expr *p_member_x(Expr *expr)
{
	if(!eat(TK_PERIOD)) return 0;
	
	while(expr->dtype->kind == PTR) {
		expr = new_deref_expr(expr);
	}
	
	Token *ident = eat(TK_IDENT);
	if(!ident)
		fatal_at(last, "expected id of member to access");
		
	Type *dtype = expr->dtype;
	
	if(dtype->kind == ARRAY && token_text_equals(ident, "length"))
		return new_member_expr(expr, ident->id, new_type(INT64));
	
	if(dtype->kind != STRUCT) {
		printf("kind %i\n", dtype->kind);
		fatal_at(expr->start, "no instance to get member");
	}
	
	Decl *struct_decl = dtype->typedecl;
	Scope *struct_scope = struct_decl->body->scope;
	Decl *decl = lookup_flat_in(ident->id, struct_scope);
	
	if(!decl)
		fatal_at(ident, "name %t not declared in struct", ident);
	
	return new_member_expr(expr, ident->id, decl->dtype);
}

static Expr *p_postfix()
{
	Expr *expr = p_atom();
	if(!expr) return 0;
	
	while(1) {
		Expr *new_expr = 0;
		(new_expr = p_cast_x(expr)) ||
		(new_expr = p_call_x(expr)) ||
		(new_expr = p_subscript_x(expr)) ||
		(new_expr = p_member_x(expr)) ;
		if(!new_expr) break;
		expr = new_expr;
	}
	
	return expr;
}

static Expr *p_ptr()
{
	if(!eat(TK_GREATER)) return 0;
	
	Expr *subexpr = p_prefix();
	
	if(!subexpr)
		fatal_after(last, "expected expression to point to");
	
	if(!subexpr->islvalue)
		fatal_at(subexpr->start, "target is not addressable");
	
	if(subexpr->kind == DEREF)
		return subexpr->subexpr;
	
	return new_ptr_expr(subexpr);
}

static Expr *p_deref()
{
	if(!eat(TK_LOWER)) return 0;
	
	Expr *subexpr = p_prefix();
	
	if(!subexpr)
		fatal_at(last, "expected expression to dereference");
		
	if(subexpr->dtype->kind != PTR)
		fatal_at(subexpr->start, "expected pointer to dereference");
	
	if(subexpr->kind == PTR)
		return subexpr->subexpr;
	
	return new_deref_expr(subexpr);
}

static Expr *p_prefix()
{
	Expr *expr = 0;
	(expr = p_ptr()) ||
	(expr = p_deref()) ||
	(expr = p_postfix()) ;
	return expr;
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
		Expr *expr = new_expr(BINOP, left->start);
		expr->left = left;
		expr->right = right;
		expr->operator = operator;
		expr->isconst = left->isconst && right->isconst;
		expr->islvalue = 0;
		Type *ltype = left->dtype;
		Type *rtype = right->dtype;
		
		if(is_integral_type(ltype) && is_integral_type(rtype)) {
			expr->dtype = new_type(INT64);
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
		
		expr = eval_binop(expr);
		left = expr;
	}
	
	return left;
}

static Expr *p_expr()
{
	return p_binop();
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
	
	if(!declare(decl))
		fatal_at(ident, "name %t already declared", ident);
	
	if(!eat(TK_LCURLY))
		fatal_after(last, "expected {");
	
	decl->body = p_stmts(decl);
	
	if(!eat(TK_RCURLY))
		fatal_after(last, "expected } after function body");
	
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
		if(!eat(TK_LCURLY))
			fatal_after(last, "expected { after else");
		stmt->else_body = p_stmts(0);
		if(!eat(TK_RCURLY))
			fatal_after(last, "expected } after else-body");
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
