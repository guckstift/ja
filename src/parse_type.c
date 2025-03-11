#ifndef parse_type_H
#define parse_type_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define parse_type_C
#endif

#include "ast.c"

Type *parse_type();

#endif
#ifdef parse_type_C

#define IMPLEMENT_FLAG

#include <stdlib.h>
#include "parse_impl.c"
#include "error.c"

Type *parse_type()
{
	Token *start = peek();

	switch(advance()->kind) {
		case KW_void:
			return create_type(TY_VOID);
		case KW_int8:
			return create_type(TY_INT8);
		case KW_int16:
			return create_type(TY_INT16);
		case KW_int32:
			return create_type(TY_INT32);
		case KW_int: case KW_int64:
			return create_type(TY_INT64);
		case KW_uint8:
			return create_type(TY_UINT8);
		case KW_uint16:
			return create_type(TY_UINT16);
		case KW_uint32:
			return create_type(TY_UINT32);
		case KW_uint: case KW_uint64:
			return create_type(TY_UINT64);
		case KW_bool:
			return create_type(TY_BOOL);
		case PT_STAR:
			return create_ptr_type(expect(K_TYPE, "expected pointer target type"));
		case KW_ptr:
			return create_ptr_type(create_type(TY_VOID));
		case KW_string:
			return create_type(TY_STRING);

		case PT_LBRACK: {
			Token *length = expect(TK_INT, "expected integer array type length");
			expect(PT_RBRACK, 0);
			Type *itemtype = expect(K_TYPE, "expected array item type");
			return create_array_type(itemtype, length ? length->ival : 0);
		}
	}

	setcur(start);
	return 0;
}

#endif