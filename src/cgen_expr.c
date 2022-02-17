#include "cgen.h"
#include "cgen_utils.h"
#include "utils.h"

static void gen_exprs(Expr **exprs);

static void gen_cast(Expr *expr)
{
	Type *dtype = expr->dtype;
	Type *subtype = dtype->subtype;
	Expr *srcexpr = expr->subexpr;
	Type *srctype = srcexpr->dtype;
	
	if(dtype->kind == BOOL) {
		write("(%e ? jatrue : jafalse)", srcexpr);
	}
	else if(is_dynarray_ptr_type(dtype)) {
		write("((%Y){.length = ", dtype);
		
		if(srctype->subtype->kind == ARRAY) {
			// from static array
			write("%i", srctype->subtype->length);
		}
		else {
			// other
			write("0");
		}
		
		write(", .items = %e})", srcexpr);
	}
	else if(srctype->kind == STRING && dtype->kind == CSTRING) {
		if(srcexpr->kind == STRING) {
			write("(\"%S\")", srcexpr->string, srcexpr->length);
		}
		else {
			write("(%e.string)", srcexpr);
		}
	}
	else {
		write("((%Y)%e)", dtype, srcexpr);
	}
}

static void gen_string(Expr *expr)
{
	write(
		"((jastring){%i, \"%S\"})", expr->length, expr->string, expr->length
	);
}

static void gen_subscript(Expr *expr)
{
	if(
		expr->subexpr->kind == DEREF &&
		is_dynarray_ptr_type(expr->subexpr->subexpr->dtype)
	) {
		Expr *dynarray = expr->subexpr->subexpr;
		Expr *index = expr->index;
		Type *itemtype = expr->dtype;
		
		write(
			"(((%y(*)%z)%e.items)[%e])",
			itemtype, itemtype, dynarray, index
		);
	}
	else {
		write("(%e[%e])", expr->subexpr, expr->index);
	}
}

static void gen_binop(Expr *expr)
{
	if(
		expr->left->dtype->kind == STRING &&
		expr->operator->type == TK_EQUALS
	) {
		write(
			"(%e.length == %e.length && "
			"memcmp(%e.string, %e.string, %e.length) == 0)",
			expr->left, expr->right,
			expr->left, expr->right, expr->left
		);
	}
	else {
		write("(%e %s %e)", expr->left, expr->operator->punct, expr->right);
	}
}

static void gen_array(Expr *expr)
{
	write("((%Y){", expr->dtype);
	gen_exprs(expr->exprs);
	write("})");
}

static void gen_call(Expr *expr)
{
	write("(%e(", expr->callee);
	gen_exprs(expr->args);
	write(")");
	
	if(expr->callee->dtype->returntype->kind == ARRAY) {
		write(".a");
	}
	
	write(")");
}

static void gen_member(Expr *expr)
{
	if(
		expr->subexpr->dtype->kind == ARRAY &&
		token_text_equals(expr->member_id, "length")
	) {
		if(
			expr->subexpr->kind == DEREF &&
			expr->subexpr->dtype->length == -1 &&
			token_text_equals(expr->member_id, "length")
		) {
			write("(%e.length)", expr->subexpr->subexpr);
		}
		else {
			write("%i", expr->subexpr->dtype->length);
		}
	}
	else {
		write("(%e.%I)", expr->subexpr, expr->member_id);
	}
}

static void gen_exprs(Expr **exprs)
{
	array_for(exprs, i) {
		if(i > 0) write(", ");
		gen_expr(exprs[i]);
	}
}

void gen_init_expr(Expr *expr)
{
	if(expr->kind == ARRAY) {
		write("{");
		array_for(expr->exprs, i) {
			if(i > 0) write(", ");
			gen_init_expr(expr->exprs[i]);
		}
		write("}");
	}
	else if(expr->kind == STRING) {
		write("{%i, \"%S\"}", expr->length, expr->string, expr->length);
	}
	else {
		gen_expr(expr);
	}
}

void gen_expr(Expr *expr)
{
	switch(expr->kind) {
		case INT:
			write("%i", expr->ival);
			break;
		case BOOL:
			write(expr->ival ? "jatrue" : "jafalse");
			break;
		case STRING:
			gen_string(expr);
			break;
		case VAR:
			write("%I", expr->id);
			break;
		case PTR:
			write("(&%e)", expr->subexpr);
			break;
		case DEREF:
			write("(*%e)", expr->subexpr);
			break;
		case CAST:
			gen_cast(expr);
			break;
		case SUBSCRIPT:
			gen_subscript(expr);
			break;
		case BINOP:
			gen_binop(expr);
			break;
		case ARRAY:
			gen_array(expr);
			break;
		case CALL:
			gen_call(expr);
			break;
		case MEMBER:
			gen_member(expr);
			break;
	}
}
