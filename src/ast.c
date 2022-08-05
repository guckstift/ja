#include <stdlib.h>
#include <assert.h>
#include "ast.h"
#include "array.h"
#include "string.h"

#include <stdio.h>

Type *new_type(Kind kind)
{
	static Type *primtypebuf[_PRIMKIND_COUNT] = {0};
	
	if(kind < _PRIMKIND_COUNT) {
		if(primtypebuf[kind] == 0) {
			Type *type = malloc(sizeof(Type));
			type->kind = kind;
			primtypebuf[kind] = type;
		}
		
		return primtypebuf[kind];
	}
	
	Type *type = malloc(sizeof(Type));
	type->kind = kind;
	return type;
}

Type *new_ptr_type(Type *subtype)
{
	static Type *primptrtypebuf[_PRIMKIND_COUNT] = {0};
	
	if(subtype) {
		Kind subkind = subtype->kind;
		
		if(subkind < _PRIMKIND_COUNT) {
			if(primptrtypebuf[subkind] == 0) {
				Type *type = new_type(PTR);
				type->subtype = new_type(subkind);
				primptrtypebuf[subkind] = type;
			}
			
			return primptrtypebuf[subkind];
		}
	}
	
	Type *type = new_type(PTR);
	type->subtype = subtype;
	return type;
}

Type *new_array_type(int64_t length, Type *itemtype)
{
	static Type *primarraytypebuf[_PRIMKIND_COUNT] = {0};
	
	if(itemtype) {
		Kind itemkind = itemtype->kind;
		
		if(length == -1 && itemkind < _PRIMKIND_COUNT) {
			if(primarraytypebuf[itemkind] == 0) {
				primarraytypebuf[itemkind] = new_type(ARRAY);
				primarraytypebuf[itemkind]->itemtype = new_type(itemkind);
				primarraytypebuf[itemkind]->length = -1;
			}
			
			return primarraytypebuf[itemkind];
		}
	}
	
	Type *type = new_type(ARRAY);
	type->itemtype = itemtype;
	type->length = length;
	return type;
}

Type *new_dynarray_type(Type *itemtype)
{
	return new_ptr_type(new_array_type(-1, itemtype));
}

Type *new_func_type(Type *returntype, Type **paramtypes)
{
	Type *type = new_type(FUNC);
	type->returntype = returntype;
	type->paramtypes = paramtypes;
	return type;
}

Type *new_struct_type(Decl *decl)
{
	Type *type = new_type(STRUCT);
	type->decl = decl;
	return type;
}

Type *new_enum_type(Decl *decl)
{
	Type *type = new_type(ENUM);
	type->decl = decl;
	return type;
}

Type *new_union_type(Decl *decl)
{
	Type *type = new_type(UNION);
	type->decl = decl;
	return type;
}

int type_equ(Type *left, Type *right)
{
	if(left->kind == PTR && right->kind == PTR) {
		return type_equ(left->subtype, right->subtype);
	}
	
	if(left->kind == ARRAY && right->kind == ARRAY) {
		return
			left->length == right->length &&
			type_equ(left->itemtype, right->itemtype);
	}
	
	if(left->kind == FUNC && right->kind == FUNC) {
		Type **lparamtypes = left->paramtypes;
		Type **rparamtypes = right->paramtypes;
		
		if(
			type_equ(left->returntype, right->returntype) &&
			array_length(lparamtypes) == array_length(rparamtypes)
		) {
			array_for(lparamtypes, i) {
				if(!type_equ(lparamtypes[i], rparamtypes[i])) {
					return 0;
				}
			}
			
			return 1;
		}
		
		return 0;
	}
	
	if(left->kind == STRUCT && right->kind == STRUCT) {
		return left->decl == right->decl;
	}
	
	if(left->kind == ENUM && right->kind == ENUM) {
		return left->decl == right->decl;
	}
	
	return left->kind == right->kind;
}

int is_integer_type(Type *type)
{
	Kind kind = type->kind;
	
	return
		kind == INT8 || kind == INT16 || kind == INT32 || kind == INT64 ||
		kind == UINT8 || kind == UINT16 || kind == UINT32 || kind == UINT64 ;
}

int is_integral_type(Type *type)
{
	return is_integer_type(type) || type->kind == BOOL;
}

int is_complete_type(Type *type)
{
	if(type->kind == ARRAY) {
		return type->length >= 0 && is_complete_type(type->itemtype);
	}
	
	if(type->kind == PTR) {
		if(type->subtype->kind == ARRAY) {
			return is_complete_type(type->subtype->itemtype);
		}
		
		return is_complete_type(type->subtype);
	}
	
	return 1;
}

int is_dynarray_ptr_type(Type *type)
{
	return
		type->kind == PTR &&
		type->subtype->kind == ARRAY &&
		type->subtype->length == -1 ;
}

Expr *new_expr(Kind kind, Token *start, Type *type, int isconst, int islvalue)
{
	Expr *expr = malloc(sizeof(Expr));
	expr->kind = kind;
	expr->start = start;
	expr->type = type;
	expr->isconst = isconst;
	expr->islvalue = islvalue;
	return expr;
}

Expr *new_int_expr(Token *start, int64_t value)
{
	Expr *expr = new_expr(INT, start, new_type(INT), 1, 0);
	expr->value = value;
	return expr;
}

Expr *new_bool_expr(Token *start, int64_t value)
{
	Expr *expr = new_expr(BOOL, start, new_type(BOOL), 1, 0);
	expr->value = value;
	return expr;
}

Expr *new_string_expr(Token *start, char *string, int64_t length)
{
	Expr *expr = new_expr(STRING, start, new_type(STRING), 1, 0);
	expr->string = string;
	expr->length = length;
	return expr;
}

Expr *new_cstring_expr(Token *start, char *string)
{
	Expr *expr = new_expr(CSTRING, start, new_type(CSTRING), 1, 0);
	expr->string = string;
	return expr;
}

Expr *new_var_expr(Token *start, Decl *decl)
{
	Type *type = decl ? decl->type : 0;
	Expr *expr = new_expr(VAR, start, type, 0, decl ? decl->kind != FUNC : 1);
	expr->decl = decl;
	expr->id = start->id;
	return expr;
}

Expr *new_array_expr(Token *start, Expr **items, int isconst)
{
	Expr *expr = new_expr(
		ARRAY, start, new_array_type(array_length(items), 0),
		isconst, 0
	);
	/*
	Expr *expr = new_expr(
		ARRAY, start, new_array_type(array_length(items), items[0]->type),
		isconst, 0
	);
	*/
	
	expr->items = items;
	return expr;
}

Expr *new_subscript_expr(Expr *array, Expr *index)
{
	Expr *expr = 0;
	
	if(array->type) {
		if(array->type->kind == STRING) {
			expr = new_expr(
				SUBSCRIPT, array->start, array->type, 0, 1
			);
		}
		else {
			expr = new_expr(
				SUBSCRIPT, array->start, array->type->itemtype, 0, 1
			);
		}
	}
	else {
		expr = new_expr(
			SUBSCRIPT, array->start, 0, 0, 1
		);
	}
	
	expr->array = array;
	expr->index = index;
	return expr;
}

Expr *new_length_expr(Expr *array)
{
	Expr *expr = new_expr(
		LENGTH, array->start, new_type(INT), array->isconst, 0
	);
	
	expr->array = array;
	return expr;
}

Expr *new_cast_expr(Expr *subexpr, Type *type)
{
	Expr *expr = new_expr(CAST, subexpr->start, type, subexpr->isconst, 0);
	expr->subexpr = subexpr;
	return expr;
}

Expr *new_member_expr(Expr *object, Decl *member)
{
	assert(member);
	
	Expr *expr = new_expr(MEMBER, object->start, member->type, 0, 1);
	expr->object = object;
	expr->member = member;
	return expr;
}

Expr *new_deref_expr(Token *start, Expr *ptr)
{
	Expr *expr = new_expr(
		DEREF, start, ptr->type ? ptr->type->subtype : 0, 0, 1
	);
	
	expr->ptr = ptr;
	return expr;
}

Expr *new_ptr_expr(Token *start, Expr *subexpr)
{
	Expr *expr = new_expr(PTR, start, new_ptr_type(subexpr->type), 0, 0);
	expr->subexpr = subexpr;
	return expr;
}

Expr *new_call_expr(Expr *callee, Expr **args)
{
	//Expr *call = new_expr(CALL, callee->start, callee->type->returntype, 0, 0);
	Expr *call = new_expr(CALL, callee->start, 0, 0, 0);
	call->callee = callee;
	call->args = args;
	return call;
}

Expr *new_binop_expr(Expr *left, Expr *right, Token *operator, OpLevel oplevel)
{
	Expr *expr = new_expr(
		BINOP, left->start, 0, left->isconst && right->isconst, 0
	);
	
	expr->left = left;
	expr->right = right;
	expr->operator = operator;
	expr->oplevel = oplevel;
	return expr;
}

Expr *new_new_expr(Token *start, Type *obj_type)
{
	Type *type = new_ptr_type(obj_type);
	return new_expr(NEW, start, type, 0, 0);
}

Expr *new_enum_item_expr(Token *start, Decl *enumdecl, EnumItem *item)
{
	assert(item);
	
	Expr *expr = new_expr(ENUM, start, enumdecl->type, 1, 0);
	expr->item = item;
	return expr;
}

#include <stdio.h>

Decl *new_decl(
	Kind kind, Token *start, Scope *scope, Token *id, int exported,
	Type *type
) {
	char *private_id = 0;
	string_append(private_id, "ja_");
	string_append_token(private_id, id);
	
	char *public_id = 0;
	string_append(public_id, "_");
	string_append(public_id, scope->unit_id);
	string_append(public_id, "_");
	string_append_token(public_id, id);
		
	Decl *decl = &new_stmt(kind, start, scope)->as_decl;
	decl->id = id;
	decl->private_id = private_id;
	decl->public_id = public_id;
	decl->imported = 0;
	decl->exported = exported;
	decl->builtin = 0;
	decl->isproto = 0;
	decl->cfunc = 0;
	decl->deps_scanned = 0;
	decl->type = type;
	return decl;
}

Decl *new_var(
	Token *start, Scope *scope, Token *id, int exported, int isparam,
	Type *type, Expr *init
) {
	Decl *decl = new_decl(VAR, start, scope, id, exported, type);
	decl->init = init;
	decl->isparam = isparam;
	return decl;
}

Decl *new_func(
	Token *start, Scope *scope, Token *id, int exported, Type *returntype,
	Decl **params
) {
	Type **paramtypes = 0;
	
	array_for(params, i) {
		array_push(paramtypes, params[i]->type);
	}
	
	Decl *decl = new_decl(FUNC, start, scope, id, exported, 0);
	decl->deps = 0;
	decl->type = new_func_type(returntype, paramtypes);
	decl->params = params;
	return decl;
}

Decl *new_struct(
	Token *start, Scope *scope, Token *id, int exported, Decl **members
) {
	Decl *decl = new_decl(STRUCT, start, scope, id, exported, 0);
	decl->type = new_struct_type(decl);
	decl->members = members;
	return decl;
}

Decl *new_enum(
	Token *start, Scope *scope, Token *id, EnumItem **items, int exported
) {
	Decl *decl = new_decl(ENUM, start, scope, id, exported, 0);
	decl->type = new_enum_type(decl);
	decl->items = items;
	return decl;
}

Decl *new_union(
	Token *start, Scope *scope, Token *id, int exported, Decl **members
) {
	Decl *decl = new_decl(UNION, start, scope, id, exported, 0);
	decl->type = new_union_type(decl);
	decl->members = members;
	return decl;
}

Decl *clone_decl(Decl *decl)
{
	Decl *new_decl = malloc(sizeof(Decl));
	*new_decl = *decl;
	return new_decl;
}

Stmt *new_stmt(Kind kind, Token *start, Scope *scope)
{
	Stmt *stmt = malloc(sizeof(Stmt));
	stmt->kind = kind;
	stmt->start = start;
	stmt->scope = scope;
	return stmt;
}

Import *new_import(Token *start, Scope *scope, Unit *unit, Decl **decls)
{
	Import *import = &new_stmt(IMPORT, start, scope)->as_import;
	import->unit = unit;
	import->decls = decls;
	return import;
}

DllImport *new_dll_import(Token *start, Scope *scope, char *name, Decl **decls)
{
	DllImport *import = &new_stmt(DLLIMPORT, start, scope)->as_dll_import;
	import->dll_name = name;
	import->decls = decls;
	return import;
}

If *new_if(Token *start, Expr *cond, Block *if_body, Block *else_body)
{
	If *ifstmt = &new_stmt(IF, start, if_body->scope->parent)->as_if;
	ifstmt->cond = cond;
	ifstmt->if_body = if_body;
	ifstmt->else_body = else_body;
	return ifstmt;
}

While *new_while(Token *start, Scope *scope, Expr *cond, Block *body)
{
	While *whilestmt = &new_stmt(WHILE, start, scope)->as_while;
	whilestmt->cond = cond;
	whilestmt->body = body;
	return whilestmt;
}

Assign *new_assign(Scope *scope, Expr *target, Expr *expr)
{
	Assign *assign = &new_stmt(ASSIGN, target->start, scope)->as_assign;
	assign->target = target;
	assign->expr = expr;
	return assign;
}

Call *new_call(Scope *scope, Expr *call)
{
	Call *stmt = &new_stmt(CALL, call->start, scope)->as_call;
	stmt->call = call;
	return stmt;
}

Print *new_print(Token *start, Scope *scope, Expr *expr)
{
	assert(expr);
	Print *print = &new_stmt(PRINT, expr->start, scope)->as_print;
	print->expr = expr;
	return print;
}

Return *new_return(Token *start, Scope *scope, Expr *expr)
{
	Return *returnstmt = &new_stmt(RETURN, start, scope)->as_return;
	returnstmt->expr = expr;
	return returnstmt;
}

Delete *new_delete(Token *start, Scope *scope, Expr *expr)
{
	Delete *stmt = &new_stmt(DELETE, start, scope)->as_delete;
	stmt->expr = expr;
	return stmt;
}

For *new_for(
	Token *start, Scope *scope, Decl *iter, Expr *from, Expr *to, Block *body
) {
	For *stmt = &new_stmt(FOR, start, scope)->as_for;
	stmt->iter = iter;
	stmt->from = from;
	stmt->to = to;
	stmt->body = body;
	return stmt;
}

ForEach *new_foreach(
	Token *start, Scope *scope, Expr *array, Decl *iter, Block *body
) {
	ForEach *stmt = &new_stmt(FOREACH, start, scope)->as_foreach;
	stmt->array = array;
	stmt->iter = iter;
	stmt->body = body;
	return stmt;
}

Scope *new_scope(char *unit_id, Scope *parent) {
	Scope *scope = malloc(sizeof(Scope));
	scope->unit_id = unit_id ? unit_id : parent ? parent->unit_id : 0;
	scope->parent = parent;
	scope->funchost = parent ? parent->funchost : 0;
	scope->structhost = 0;
	scope->loophost = parent ? parent->loophost : 0;
	scope->imports = 0;
	scope->dll_imports = 0;
	scope->decls = 0;
	return scope;
}

Decl *lookup_flat_in(Token *id, Scope *scope)
{
	array_for(scope->decls, i) {
		if(scope->decls[i]->id == id) {
			return scope->decls[i];
		}
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

bool scope_contains_scope(Scope *upper, Scope *lower)
{
	if(lower->parent) {
		if(lower->parent == upper) return true;
		return scope_contains_scope(upper, lower->parent);
	}
	
	return false;
}

int declare_in(Decl *decl, Scope *scope)
{
	if(decl->scope != scope) {
		decl = clone_decl(decl);
		decl->imported = 1;
		decl->exported = 0;
	}
	
	if(lookup_flat_in(decl->id, scope)) {
		return 0;
	}
	
	array_push(scope->decls, decl);
	return 1;
}

int redeclare_in(Decl *decl, Scope *scope)
{
	array_for(scope->decls, i) {
		if(scope->decls[i]->id == decl->id) {
			scope->decls[i] = decl;
			return 1;
		}
	}
	
	return 0;
}

Block *new_block(Stmt **stmts, Scope *scope)
{
	Block *block = malloc(sizeof(Block));
	block->stmts = stmts;
	block->scope = scope;
	return block;
}
