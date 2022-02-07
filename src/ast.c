#include <stdlib.h>
#include "ast.h"

TypeDesc *new_type(Type type)
{
	TypeDesc *dtype = malloc(sizeof(TypeDesc));
	dtype->type = type;
	return dtype;
}

TypeDesc *new_ptr_type(TypeDesc *subtype)
{
	TypeDesc *dtype = malloc(sizeof(TypeDesc));
	dtype->type = TY_PTR;
	dtype->subtype = subtype;
	return dtype;
}

TypeDesc *new_array_type(int64_t length, TypeDesc *subtype)
{
	TypeDesc *dtype = malloc(sizeof(TypeDesc));
	dtype->type = TY_ARRAY;
	dtype->subtype = subtype;
	dtype->length = length;
	return dtype;
}

TypeDesc *new_func_type(TypeDesc *returntype)
{
	TypeDesc *dtype = malloc(sizeof(TypeDesc));
	dtype->type = TY_FUNC;
	dtype->returntype = returntype;
	return dtype;
}

int type_equ(TypeDesc *dtype1, TypeDesc *dtype2)
{
	if(dtype1->type == TY_PTR && dtype2->type == TY_PTR) {
		return type_equ(dtype1->subtype, dtype2->subtype);
	}
	
	if(dtype1->type == TY_ARRAY && dtype2->type == TY_ARRAY) {
		return
			dtype1->length == dtype2->length &&
			type_equ(dtype1->subtype, dtype2->subtype);
	}
	
	if(dtype1->type == TY_STRUCT && dtype2->type == TY_STRUCT) {
		return dtype1->id == dtype2->id;
	}
	
	return dtype1->type == dtype2->type;
}

int is_integer_type(TypeDesc *dtype)
{
	Type type = dtype->type;
	return type == TY_INT64 || type == TY_UINT8 || type == TY_UINT64;
}

int is_integral_type(TypeDesc *dtype)
{
	Type type = dtype->type;
	return is_integer_type(dtype) || type == TY_BOOL;
}

int is_complete_type(TypeDesc *dt)
{
	if(dt->type == TY_PTR) {
		if(dt->subtype->type == TY_ARRAY) {
			return is_complete_type(dt->subtype->subtype);
		}
		
		return is_complete_type(dt->subtype);
	}
	
	if(dt->type == TY_ARRAY) {
		return dt->length >= 0 && is_complete_type(dt->subtype);
	}
	
	return 1;
}

int is_dynarray_ptr_type(TypeDesc *dt)
{
	return
		dt->type == TY_PTR &&
		dt->subtype->type == TY_ARRAY &&
		dt->subtype->length == -1 ;
}

Expr *new_expr(ExprType type, Token *start)
{
	Expr *expr = malloc(sizeof(Expr));
	expr->type = type;
	expr->start = start;
	expr->next = 0;
	return expr;
}

Expr *new_int_expr(int64_t val, Token *start)
{
	Expr *expr = new_expr(EX_INT, start);
	expr->ival = val;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(TY_INT64);
	return expr;
}

Expr *new_bool_expr(int64_t val, Token *start)
{
	Expr *expr = new_expr(EX_BOOL, start);
	expr->ival = val;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(TY_BOOL);
	return expr;
}

Expr *new_var_expr(Token *id, TypeDesc *dtype, Token *start)
{
	Expr *expr = new_expr(EX_VAR, start);
	expr->id = id;
	expr->isconst = 0;
	expr->islvalue = dtype->type != TY_FUNC;
	expr->dtype = dtype;
	return expr;
}

Expr *new_subscript(Expr *subexpr, Expr *index)
{
	Expr *expr = new_expr(EX_SUBSCRIPT, subexpr->start);
	expr->subexpr = subexpr;
	expr->index = index;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = subexpr->dtype->subtype;
	return expr;
}

Expr *new_cast_expr(Expr *subexpr, TypeDesc *dtype)
{
	Expr *expr = new_expr(EX_CAST, subexpr->start);
	expr->isconst = subexpr->isconst;
	expr->islvalue = 0;
	expr->subexpr = subexpr;
	expr->dtype = dtype;
	return expr;
}

Stmt *new_stmt(StmtType type, Token *start, Scope *scope)
{
	Stmt *stmt = malloc(sizeof(Stmt));
	stmt->type = type;
	stmt->start = start;
	stmt->next = 0;
	stmt->scope = scope;
	return stmt;
}

Stmt *new_assign(Expr *target, Expr *expr, Scope *scope)
{
	Stmt *stmt = new_stmt(ST_ASSIGN, target->start, scope);
	stmt->target = target;
	stmt->expr = expr;
	return stmt;
}

Stmt *lookup_flat_in(Token *id, Scope *scope)
{
	for(Stmt *decl = scope->first_decl; decl; decl = decl->next_decl) {
		if(decl->id == id) return decl;
	}
	
	return 0;
}

Stmt *lookup_in(Token *id, Scope *scope)
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
