#ifndef gen_type_H
#define gen_type_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define gen_type_C
#endif

#include <stdarg.h>
#include <stdio.h>

char *write_type_prefix(FILE *fs, char *msg, va_list args);
char *write_type_postfix(FILE *fs, char *msg, va_list args);
char *write_full_type(FILE *fs, char *msg, va_list args);

#endif
#ifdef gen_type_C

#define IMPLEMENT_FLAG

#include "gen_impl.c"
#include "ast.c"
#include "error.c"

char *write_type_prefix(FILE *fs, char *msg, va_list args)
{
	Type *type = va_arg(args, Type*);

	switch(type->kind) {
		case TY_VOID:
			write("void");
			break;
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
		case TY_STRING:
			write("jastring");
			break;

		case TY_PTR:
			write("%y(*", type->subtype);
			break;

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
	Type *type = va_arg(args, Type*);
	write("%y%z", type, type);
	return msg + 1;
}

#endif