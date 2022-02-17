#include <stdlib.h>
#include "ast.h"
#include "utils.h"

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
	if(!subtype) subtype = new_type(NONE);
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

Type *new_dynarray_type(Type *itemtype)
{
	return new_ptr_type(new_array_type(-1, itemtype));
}

Type *new_func_type(Type *returntype, Decl *func)
{
	Type *dtype = malloc(sizeof(Type));
	dtype->kind = FUNC;
	dtype->returntype = returntype;
	dtype->func = func;
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
	return
		kind == INT8 || kind == INT16 || kind == INT32 || kind == INT64 ||
		kind == UINT8 || kind == UINT16 || kind == UINT32 || kind == UINT64 ;
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

Expr *new_expr(Kind kind, Token *start)
{
	Expr *expr = malloc(sizeof(Expr));
	expr->kind = kind;
	expr->start = start;
	return expr;
}

Expr *new_int_expr(int64_t val, Token *start)
{
	Expr *expr = new_expr(INT, start);
	expr->ival = val;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(INT64);
	return expr;
}

Expr *new_string_expr(char *string, int64_t length, Token *start)
{
	Expr *expr = new_expr(STRING, start);
	expr->string = string;
	expr->length = length;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(STRING);
	return expr;
}

Expr *new_bool_expr(int64_t val, Token *start)
{
	Expr *expr = new_expr(BOOL, start);
	expr->ival = val;
	expr->isconst = 1;
	expr->islvalue = 0;
	expr->dtype = new_type(BOOL);
	return expr;
}

Expr *new_var_expr(Token *id, Type *dtype, Decl *decl, Token *start)
{
	Expr *expr = new_expr(VAR, start);
	expr->id = id;
	expr->isconst = 0;
	expr->islvalue = dtype->kind != FUNC;
	expr->dtype = dtype;
	expr->decl = decl;
	return expr;
}

Expr *new_array_expr(
	Expr **exprs, int64_t length, int isconst, Type *itemtype, Token *start
) {
	Expr *expr = new_expr(ARRAY, start);
	expr->exprs = exprs;
	expr->length = length;
	expr->isconst = isconst;
	expr->islvalue = 0;
	expr->dtype = new_array_type(length, itemtype);
	return expr;
}

Expr *new_subscript(Expr *subexpr, Expr *index)
{
	Expr *expr = new_expr(SUBSCRIPT, subexpr->start);
	expr->subexpr = subexpr;
	expr->index = index;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = subexpr->dtype->itemtype;
	return expr;
}

Expr *new_cast_expr(Expr *subexpr, Type *dtype)
{
	Expr *expr = new_expr(CAST, subexpr->start);
	expr->isconst = subexpr->isconst;
	expr->islvalue = 0;
	expr->subexpr = subexpr;
	expr->dtype = dtype;
	return expr;
}

Expr *new_member_expr(Expr *subexpr, Token *member_id, Type *dtype)
{
	Expr *expr = new_expr(MEMBER, subexpr->start);
	expr->member_id =member_id;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = dtype;
	expr->subexpr = subexpr;
	return expr;
}

Expr *new_deref_expr(Expr *subexpr)
{
	Expr *expr = new_expr(DEREF, subexpr->start);
	expr->subexpr = subexpr;
	expr->isconst = 0;
	expr->islvalue = 1;
	expr->dtype = subexpr->dtype->subtype;
	return expr;
}

Expr *new_ptr_expr(Expr *subexpr)
{
	Expr *expr = new_expr(PTR, subexpr->start);
	expr->subexpr = subexpr;
	expr->isconst = 0;
	expr->islvalue = 0;
	expr->dtype = new_ptr_type(subexpr->dtype);
	return expr;
}

Stmt *clone_stmt(Stmt *stmt)
{
	Stmt *new_stmt = malloc(sizeof(Stmt));
	*new_stmt = *stmt;
	return new_stmt;
}

Stmt *new_stmt(Kind kind, Token *start, Scope *scope)
{
	Stmt *stmt = malloc(sizeof(Stmt));
	stmt->kind = kind;
	stmt->start = start;
	stmt->scope = scope;
	stmt->as_decl.exported = 0;
	stmt->as_decl.imported = 0;
	return stmt;
}

Assign *new_assign(Expr *target, Expr *expr, Scope *scope)
{
	Assign *assign = (Assign*)new_stmt(ASSIGN, target->start, scope);
	assign->target = target;
	assign->expr = expr;
	return assign;
}

Decl *new_vardecl(
	Token *id, Type *dtype, Expr *init, Token *start, Scope *scope
) {
	Decl *decl = (Decl*)new_stmt(VAR, start, scope);
	decl->id = id;
	decl->dtype = dtype;
	decl->init = init;
	decl->builtin = 0;
	return decl;
}

Decl *new_funcdecl(
	Token *id, Type *dtype, int exported, Decl **params, Stmt **body,
	int isproto, Token *start, Scope *scope
) {
	Decl *decl = (Decl*)new_stmt(FUNC, start, scope);
	decl->exported = exported;
	decl->id = id;
	decl->dtype = dtype;
	decl->params = params;
	decl->body = body;
	decl->isproto = isproto;
	decl->builtin = 0;
	return decl;
}

Import *new_import(Token *filename, Unit *unit, Token *start, Scope *scope)
{
	Import *import = (Import*)new_stmt(IMPORT, start, scope);
	import->filename = filename;
	import->unit = unit;
	return import;
}

Decl *lookup_flat_in(Token *id, Scope *scope)
{
	array_for(scope->decls, i) {
		if(scope->decls[i]->id == id) return scope->decls[i];
	}
	
	return 0;
}

Decl *lookup_in(Token *id, Scope *scope)
{
	Decl *decl = lookup_flat_in(id, scope);
	
	if(decl) {
		return decl;
	}
	
	if(scope->parent) {
		return lookup_in(id, scope->parent);
	}
	
	return 0;
}
