#include "cgen_internal.h"

static void gen_struct_or_enum_type(Type *type)
{
	if(type->decl->exported && is_in_header()) {
		write("%X", type->decl->id);
	}
	else if(
		type->decl->scope != get_cur_unit()->block->stmts[0]->scope &&
		type->decl->imported == 0
	) {
		write("%s", type->decl->public_id);
	}
	else {
		write("%I", type->decl->id);
	}
}

static void gen_ptr_type(Type *type)
{
	if(is_dynarray_ptr_type(type)) {
		write("jadynarray");
	}
	else {
		gen_type(type->subtype);
		if(type->subtype->kind == ARRAY) write("(");
		write("*");
	}
}

void gen_type_postfix(Type *type)
{
	switch(type->kind) {
		case PTR:
			if(is_dynarray_ptr_type(type)) break;
			if(type->subtype->kind == ARRAY) write(")");
			gen_type_postfix(type->subtype);
			break;
		case ARRAY:
			write("[%u]%z", type->length, type->itemtype);
			break;
		case FUNC:
			write(")");
			// TODO: gen param part
			write("()");
			break;
	}
}

void gen_type(Type *type)
{
	if(!type) {
		write("/* nulltype */");
		return;
	}
	
	switch(type->kind) {
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
		case CSTRING:
			write("char*");
			break;
		case SLICE:
			write("jaslice");
			break;
		case STRUCT:
			gen_struct_or_enum_type(type);
			break;
		case UNION:
			gen_struct_or_enum_type(type);
			break;
		case ENUM:
			gen_struct_or_enum_type(type);
			break;
		case PTR:
			gen_ptr_type(type);
			break;
		case ARRAY:
			gen_type(type->itemtype);
			break;
		case FUNC:
			gen_type(type->returntype);
			write("(");
			break;
	}
}
