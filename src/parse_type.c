#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "parse.h"
#include "parse_internal.h"
#include "eval.h"
#include "build.h"
#include "array.h"

static Type *p_type();

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
	if(eat(TK_cstring)) return new_type(CSTRING);
	if(eat(TK_ptr)) return new_ptr_type(new_type(NONE));
	return 0;
}

static Type *p_nametype()
{
	Token *ident = eat(TK_IDENT);
	if(!ident) return 0;
	
	Decl *decl = lookup(ident->id);
	
	if(!decl)
		fatal_at(ident, "name %t not declared", ident);
	
	if(decl->kind != STRUCT && decl->kind != ENUM && decl->kind != UNION)
		fatal_at(ident, "%t is not a structure, union or enum", ident);
	
	return decl->type;
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
	Type *type = 0;
	(type = p_primtype()) ||
	(type = p_nametype()) ||
	(type = p_ptrtype()) ||
	(type = p_arraytype()) ;
	return type;
}

void make_type_exportable(Type *type)
{
	while(type->kind == PTR || type->kind == ARRAY) {
		type = type->subtype;
	}
	
	if(type->kind == STRUCT) {
		Decl *decl = type->decl;
		
		if(decl->exported == 0) {
			decl->exported = 1;
			Decl **members = decl->members;
			
			array_for(members, i) {
				make_type_exportable(members[i]->type);
			}
		}
	}
	else if(type->kind == FUNC) {
		make_type_exportable(type->returntype);
			
		array_for(type->paramtypes, i) {
			make_type_exportable(type->paramtypes[i]);
		}
	}
}

void complete_type(Type *type, Expr *expr)
{
	// automatic array length completion from expr
	for(
		Type *dt = type, *st = expr->type;
		dt->kind == ARRAY && st->kind == ARRAY;
		dt = dt->itemtype, st = st->itemtype
	) {
		if(dt->length == -1) {
			dt->length = st->length;
		}
	}
}

Type *p_type_pub(ParseState *state)
{
	unpack_state(state);
	Type *type = p_type();
	pack_state(state);
	return type;
}
