#ifndef gen_expr_H
#define gen_expr_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define gen_expr_C
#endif

#include <stdarg.h>
#include <stdio.h>

char *write_expr(FILE *fs, char *msg, va_list args);
char *write_init_expr(FILE *fs, char *msg, va_list args);
char *write_string_literal(FILE *fs, char *msg, va_list args);

#endif
#ifdef gen_expr_C

#define IMPLEMENT_FLAG

#include "gen_impl.c"
#include "ast.c"
#include "error.c"
#include "array.c"
#include "string.c"

char *write_string_literal(FILE *fs, char *msg, va_list args)
{
	uint8_t *strlit = va_arg(args, uint8_t*);
	fprintf(fs, "\"");

	for(int64_t i=0; i < string_length(strlit); i++) {
		if(strlit[i] == '\n')
			fprintf(fs, "\\n");
		else if(strlit[i] == '\t')
			fprintf(fs, "\\t");
		else if(strlit[i] == '"')
			fprintf(fs, "\\\"");
		else if(strlit[i] == '\\')
			fprintf(fs, "\\\\");
		else if(strlit[i] < 0x10 || strlit[i] >= 0x7f)
			fprintf(fs, "\\x%c%c\"\"", HEXCHARS[strlit[i] >> 4], HEXCHARS[strlit[i] & 15]);
		else
			fprintf(fs, "%c", strlit[i]);
	}

	fprintf(fs, "\"");
	return msg + 1;
}

char *write_expr(FILE *fs, char *msg, va_list args)
{
	Expr *expr = va_arg(args, Expr*);

	switch(expr->kind) {
		case EX_INT:
			write("INT64_C(%i)", expr->ival);
			break;
		case EX_BOOL:
			write(expr->bval ? "1" : "0");
			break;
		case EX_NULL:
			write("NULL");
			break;
		case EX_STRING:
			write("(jastring){.chars = %S, .length = %i}", expr->sval, string_length(expr->sval));
			break;
		case EX_VAR:
			write("%j", expr->decl->uid);
			break;
		case EX_PTR:
			write("&(%e)", expr->subexpr);
			break;
		case EX_DEREF:
			write("*(%e)", expr->subexpr);
			break;

		case EX_CAST: {
			if(expr->type->kind == TY_BOOL)
				write("(%e) != 0", expr->subexpr);
			else
				write("%e", expr->subexpr);
		} break;

		case EX_SUBSCRIPT:
			write("%e[%e]", expr->subexpr, expr->index);
			break;

		case EX_CALL: {
			write("%e(", expr->callee);

			array_for(expr->args, i) {
				if(i > 0) write(", ");
				write("%e", expr->args[i]);
			}

			write(")");
		} break;

		default:
			error_at(expr, "INTERNAL: unhandled expression to generate (%i)", expr->kind);
	}

	return msg + 1;
}

char *write_init_expr(FILE *fs, char *msg, va_list args)
{
	Expr *expr = va_arg(args, Expr*);

	if(expr->kind == EX_STRING)
		write("{.chars = %S, .length = %i}", expr->sval, string_length(expr->sval));
	else
		write("%e", expr);

	return msg + 1;
}

#endif