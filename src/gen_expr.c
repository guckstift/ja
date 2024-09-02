#include "gen_expr.h"
#include "gen_impl.h"
#include "ast.h"
#include "error.h"

char *write_expr(FILE *fs, char *msg, va_list args)
{
	Expr *expr = va_arg(args, Expr*);

	switch(expr->kind) {
		case EX_INT:
			write("INT64_C(%i)", expr->ival);
			break;
		case EX_BOOL:
			write("%s", expr->bval ? "1" : "0");
			break;
		case EX_NULL:
			write("0");
			break;
		case EX_VAR:
			write("%j", expr->decl->id);
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

		default:
			error_at(expr, "INTERNAL: unhandled expression to generate (%i)", expr->kind);
	}

	return msg + 1;
}