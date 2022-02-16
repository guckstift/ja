#include "cgen.h"
#include "cgen_utils.h"

static void gen_struct_type(Type *dtype)
{
	if(dtype->typedecl->exported && is_in_header()) {
		write("%X", dtype->id);
	}
	else if(
		dtype->typedecl->scope != get_cur_unit()->stmts[0]->scope &&
		dtype->typedecl->imported == 0
	) {
		write("%s", dtype->typedecl->public_id);
	}
	else {
		write("%I", dtype->id);
	}
}

static void gen_ptr_type(Type *dtype)
{
	if(is_dynarray_ptr_type(dtype)) {
		write("jadynarray");
	}
	else {
		gen_type(dtype->subtype);
		if(dtype->subtype->kind == ARRAY) write("(");
		write("*");
	}
}

void gen_type_postfix(Type *dtype)
{
	switch(dtype->kind) {
		case PTR:
			if(is_dynarray_ptr_type(dtype)) break;
			if(dtype->subtype->kind == ARRAY) write(")");
			gen_type_postfix(dtype->subtype);
			break;
		case ARRAY:
			write("[%u]%z", dtype->length, dtype->itemtype);
			break;
	}
}

void gen_type(Type *dtype)
{
	switch(dtype->kind) {
		case NONE:
			write("void");
			break;
		case INT8:
			write("int8_t");
			break;
		case INT16:
			write("int16_t");
			break;
		case INT32:
			write("int32_t");
			break;
		case INT64:
			write("int64_t");
			break;
		case UINT8:
			write("uint8_t");
			break;
		case UINT16:
			write("uint16_t");
			break;
		case UINT32:
			write("uint32_t");
			break;
		case UINT64:
			write("uint64_t");
			break;
		case BOOL:
			write("jabool");
			break;
		case STRING:
			write("jastring");
			break;
		case STRUCT:
			gen_struct_type(dtype);
			break;
		case PTR:
			gen_ptr_type(dtype);
			break;
		case ARRAY:
			gen_type(dtype->itemtype);
			break;
	}
}
