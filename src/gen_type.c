#include "gen_type.h"
#include "gen_impl.h"
#include "ast.h"
#include "error.h"

char *write_type(FILE *fs, char *msg, va_list args)
{
	Type *type = va_arg(args, Type*);

	switch(type->kind) {
		case TY_INT8:
			write("int8_t");
			break;
		case TY_INT16:
			write("int16_t");
			break;
		case TY_INT32:
			write("int32_t");
			break;
		case TY_INT64:
			write("int64_t");
			break;
		case TY_UINT8:
			write("uint8_t");
			break;
		case TY_UINT16:
			write("uint16_t");
			break;
		case TY_UINT32:
			write("uint32_t");
			break;
		case TY_UINT64:
			write("uint64_t");
			break;
		case TY_BOOL:
			write("int8_t");
			break;

		case TY_PTR: {
			if(type->subtype)
				write("%y(*", type->subtype);
			else
				write("void*");
		} break;

		case TY_ARRAY:
			write("%y", type->subtype);
			break;

		default:
			error("INTERNAL: unhandled type to generate (%i)", type->kind);
	}

	return msg + 1;
}

char *write_type_postfix(FILE *fs, char *msg, va_list args)
{
	Type *type = va_arg(args, Type*);

	switch(type->kind) {
		case TY_PTR:
			write(")%z", type->subtype);
			break;
		case TY_ARRAY:
			write("[%i]%z", type->length, type->subtype);
			break;
	}

	return msg + 1;
}

char *write_full_type(FILE *fs, char *msg, va_list args)
{
	va_list args2;
	va_copy(args2, args);
	write_type(fs, msg, args);
	write_type_postfix(fs, msg, args2);
	va_end(args2);
	return msg + 1;
}