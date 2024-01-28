#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "parse.h"
#include "parse_internal.h"
#include "build.h"
#include "utils/array.h"

static Type *p_primtype()
{
	if(eatkw(KW_int)) return new_type(INT);
	if(eatkw(KW_int8)) return new_type(INT8);
	if(eatkw(KW_int16)) return new_type(INT16);
	if(eatkw(KW_int32)) return new_type(INT32);
	if(eatkw(KW_int64)) return new_type(INT64);
	if(eatkw(KW_uint)) return new_type(UINT);
	if(eatkw(KW_uint8)) return new_type(UINT8);
	if(eatkw(KW_uint16)) return new_type(UINT16);
	if(eatkw(KW_uint32)) return new_type(UINT32);
	if(eatkw(KW_uint64)) return new_type(UINT64);
	if(eatkw(KW_bool)) return new_type(BOOL);
	if(eatkw(KW_string)) return new_type(STRING);
	if(eatkw(KW_cstring)) return new_type(CSTRING);
	if(eatkw(KW_ptr)) return new_ptr_type(new_type(NONE));
	return 0;
}

static Type *p_nametype()
{
	Token *ident = eat(TK_IDENT);
	if(!ident) return 0;
	return new_named_type(ident->id);
}

static Type *p_ptrtype()
{
	if(!eatpt(PT_MUL)) return 0;

	Type *subtype = p_(type);
	if(!subtype)
		fatal_at(last, "expected target type");

	return new_ptr_type(subtype);
}

static Type *p_arraytype()
{
	if(!eatpt(PT_LBRACK)) return 0;

	Token *length = eat(TK_INT);

	if(length && length->ival <= 0)
		fatal_at(length, "array length must be greater than 0");

	if(!length && !eatpt(PT_MUL)) {
		fatal_after(last, "expected integer literal for array length or *");
	}

	if(!eatpt(PT_RBRACK)) {
		fatal_after(last, "expected ]");
	}

	Type *itemtype = p_(type);
	if(!itemtype)
		fatal_at(last, "expected item type");

	if(length)
		return new_array_type(length->ival, itemtype);
	else
		return new_slice_type(itemtype);
}

Type *p_type()
{
	Type *type = 0;
	(type = p_(primtype)) ||
	(type = p_(nametype)) ||
	(type = p_(ptrtype)) ||
	(type = p_(arraytype)) ;
	return type;
}
