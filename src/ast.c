#include <stdlib.h>
#include "ast.h"

Type *new_type(Kind kind)
{
	static Type *primtypebuf[_PRIMKIND_COUNT] = {0};
	
	if(kind < _PRIMKIND_COUNT) {
		if(primtypebuf[kind] == 0) {
			primtypebuf[kind] = malloc(sizeof(Type));
			primtypebuf[kind]->kind = kind;
		}
		
		return primtypebuf[kind];
	}
	
	Type *dtype = malloc(sizeof(Type));
	dtype->kind = kind;
	return dtype;
}

Type *new_ptr_type(Type *subtype)
{
	Type *dtype = malloc(sizeof(Type));
	dtype->kind = PTR;
	dtype->subtype = subtype;
	return dtype;
}

Type *new_array_type(int64_t length, Type *itemtype)
{
	Type *dtype = malloc(sizeof(Type));
	dtype->kind = ARRAY;
	dtype->itemtype = itemtype;
	dtype->length = length;
	return dtype;
}

Type *new_func_type(Type *returntype)
{
	Type *dtype = malloc(sizeof(Type));
	dtype->kind = FUNC;
	dtype->returntype = returntype;
	return dtype;
}

int type_equ(Type *dtype1, Type *dtype2)
{
	if(dtype1->kind == PTR && dtype2->kind == PTR) {
		return type_equ(dtype1->subtype, dtype2->subtype);
	}
	
	if(dtype1->kind == ARRAY && dtype2->kind == ARRAY) {
		return
			dtype1->length == dtype2->length &&
			type_equ(dtype1->itemtype, dtype2->itemtype);
	}
	
	if(dtype1->kind == STRUCT && dtype2->kind == STRUCT) {
		return dtype1->id == dtype2->id;
	}
	
	return dtype1->kind == dtype2->kind;
}

int is_integer_type(Type *dtype)
{
	Kind kind = dtype->kind;
	return kind == INT64 || kind == UINT8 || kind == UINT64;
}

int is_integral_type(Type *dtype)
{
	return is_integer_type(dtype) || dtype->kind == BOOL;
}

int is_complete_type(Type *dt)
{
	if(dt->kind == PTR) {
		if(dt->subtype->kind == ARRAY) {
			return is_complete_type(dt->subtype->itemtype);
		}
		
		return is_complete_type(dt->subtype);
	}
	
	if(dt->kind == ARRAY) {
		return dt->length >= 0 && is_complete_type(dt->itemtype);
	}
	
	return 1;
}

int is_dynarray_ptr_type(Type *dtype)
{
	return
		dtype->kind == PTR &&
		dtype->subtype->kind == ARRAY &&
		dtype->subtype->length == -1 ;
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
	expr->dtype = new_type(INT64);
	return expr;
}

Expr *new_string_expr(char *string, int64_t length, Token *start)
{
	Expr *expr = new_expr(EX_STRING, start);
	expr->string = string;
	expr->length = length;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(STRING);
	return expr;
}

Expr *new_bool_expr(int64_t val, Token *start)
{
	Expr *expr = new_expr(EX_BOOL, start);
	expr->ival = val;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(BOOL);
	return expr;
}

Expr *new_var_expr(Token *id, Type *dtype, Token *start)
{
	Expr *expr = new_expr(EX_VAR, start);
	expr->id = id;
	expr->isconst = 0;
	expr->islvalue = dtype->kind != FUNC;
	expr->dtype = dtype;
	return expr;
}

Expr *new_array_expr(
	Expr *exprs, int64_t length, int isconst, Type *itemtype, Token *start
) {
	Expr *expr = new_expr(EX_ARRAY, start);
	expr->exprs = exprs;
	expr->length = length;
	expr->isconst = isconst;
	expr->islvalue = 0;
	expr->dtype = new_array_type(length, itemtype);
	return expr;
}

Expr *new_subscript(Expr *subexpr, Expr *index)
{
	Expr *expr = new_expr(EX_SUBSCRIPT, subexpr->start);
	expr->subexpr = subexpr;
	expr->index = index;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = subexpr->dtype->itemtype;
	return expr;
}

Expr *new_cast_expr(Expr *subexpr, Type *dtype)
{
	Expr *expr = new_expr(EX_CAST, subexpr->start);
	expr->isconst = subexpr->isconst;
	expr->islvalue = 0;
	expr->subexpr = subexpr;
	expr->dtype = dtype;
	return expr;
}

Expr *new_member_expr(Expr *subexpr, Token *member_id, Type *dtype)
{
	Expr *expr = new_expr(EX_MEMBER, subexpr->start);
	expr->member_id =member_id;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = dtype;
	expr->subexpr = subexpr;
	return expr;
}

Expr *new_deref_expr(Expr *subexpr)
{
	Expr *expr = new_expr(EX_DEREF, subexpr->start);
	expr->subexpr = subexpr;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = subexpr->dtype->subtype;
	return expr;
}

Expr *new_ptr_expr(Expr *subexpr)
{
	Expr *expr = new_expr(EX_PTR, subexpr->start);
	expr->subexpr = subexpr;
	expr->isconst = 0;
	expr->islvalue = 0;
	expr->dtype = new_ptr_type(subexpr->dtype);
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
